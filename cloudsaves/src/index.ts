// The cloud saves Worker. Each phrase holds one save, which a player uploads on one device and loads on another. See
// ui-rework/cloud-saves.md.
//
//   PUT /v1/save        stores the request's body, when it's a save from a current Modern Morrow build
//   GET /v1/save        returns the stored save
//   GET /v1/save/info   returns the stored save's details, as JSON
//
// Every request carries the phrase in an "Authorization: Phrase <phrase>" header.
//
// The bucket holds two kinds of file for each phrase, named after the phrase's hash:
//   <hash>/<random id>   a save. Each upload gets a new name, so a refused upload never replaces a good save.
//   <hash>.json          which of those saves is the phrase's, and its details. Writing it is what makes an upload
//                        the phrase's save, since R2 can't rename a file.
import { saveDetails, type SaveDetails } from "./details";
import { phraseFromHeader, saveKey } from "./phrase";
import {
  checkEnd,
  checkStart,
  END_READ_BYTES,
  InvalidSave,
  MAX_SAVE_BYTES,
  MIN_SAVE_BYTES,
  START_READ_BYTES,
  startLength,
} from "./savecheck";

interface Current {
  file: string;
  details: SaveDetails;
}

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

// Errors are short JSON, so the game can show the player what went wrong.
function error(status: number, message: string): Response {
  return json({ error: message }, status);
}

const currentName = (key: string) => `${key}.json`;

async function readCurrent(env: Env, key: string): Promise<Current | null> {
  const object = await env.SAVES.get(currentName(key));
  return object === null ? null : object.json<Current>();
}

async function readRange(env: Env, file: string, range: R2Range): Promise<Uint8Array> {
  const object = await env.SAVES.get(file, { range });
  if (object === null) throw new Error(`${file} disappeared while it was checked`);
  return new Uint8Array(await object.arrayBuffer());
}

// Checks a stored save from its start and its end, without reading the rest. See src/savecheck.ts.
async function checkStored(env: Env, file: string, size: number) {
  let start = await readRange(env, file, { offset: 0, length: Math.min(size, START_READ_BYTES) });
  const needed = startLength(start, size);
  if (needed > start.byteLength) start = await readRange(env, file, { offset: 0, length: Math.min(size, needed) });
  const info = checkStart(start, size);
  checkEnd(await readRange(env, file, { suffix: Math.min(size, END_READ_BYTES) }));
  return info;
}

async function upload(request: Request, env: Env, key: string): Promise<Response> {
  // Refuses a wrong size before storing anything. A request without a length could be of any size.
  const length = request.headers.get("Content-Length");
  if (length === null) return error(411, "the upload has no Content-Length");
  const size = Number(length);
  if (size > MAX_SAVE_BYTES) return error(413, "the save is too large");
  if (!(size >= MIN_SAVE_BYTES) || request.body === null) return error(400, "the file is too small");

  // R2 takes the body straight from the request, so none of its bytes pass through this code. Reading 15 MB here
  // would use more CPU time than the free plan allows.
  const file = `${key}/${crypto.randomUUID()}`;
  const stored = await env.SAVES.put(file, request.body, { httpMetadata: { contentType: "application/octet-stream" } });
  let details;
  try {
    details = saveDetails(await checkStored(env, file, stored.size), stored.size, new Date());
  } catch (e) {
    await env.SAVES.delete(file);
    if (e instanceof InvalidSave) return error(400, e.message);
    throw e;
  }

  const previous = await readCurrent(env, key);
  const current: Current = { file, details };
  await env.SAVES.put(currentName(key), JSON.stringify(current), { httpMetadata: { contentType: "application/json" } });
  if (previous !== null && previous.file !== file) await env.SAVES.delete(previous.file);
  return json(details);
}

async function download(env: Env, key: string): Promise<Response> {
  const current = await readCurrent(env, key);
  const object = current === null ? null : await env.SAVES.get(current.file);
  if (object === null) return error(404, "this phrase holds no save");
  return new Response(object.body, {
    headers: { "Content-Type": "application/octet-stream", "Content-Length": String(object.size) },
  });
}

async function info(env: Env, key: string): Promise<Response> {
  const current = await readCurrent(env, key);
  if (current === null) return error(404, "this phrase holds no save");
  return json(current.details);
}

export default {
  async fetch(request, env): Promise<Response> {
    const route = `${request.method} ${new URL(request.url).pathname}`;
    if (route !== "PUT /v1/save" && route !== "GET /v1/save" && route !== "GET /v1/save/info")
      return error(404, "no such request");

    // Counts requests per IP address. Uploads have a lower limit than reads.
    const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
    const limiter = request.method === "PUT" ? env.UPLOAD_LIMIT : env.READ_LIMIT;
    if (!(await limiter.limit({ key: ip })).success) return error(429, "too many requests, try again in a minute");

    const phrase = phraseFromHeader(request.headers.get("Authorization"));
    if (phrase === null) return error(401, "the request has no phrase");
    const key = await saveKey(phrase);

    if (route === "PUT /v1/save") return upload(request, env, key);
    if (route === "GET /v1/save") return download(env, key);
    return info(env, key);
  },
} satisfies ExportedHandler<Env>;
