// Tests of the Worker's three requests. They run in Cloudflare's runtime, against its local copy of R2.
import { env, exports } from "cloudflare:workers";
import { describe, expect, it } from "vitest";
import { saveKey } from "../../src/phrase";
import { MAX_SAVE_BYTES, START_READ_BYTES } from "../../src/savecheck";
import { save, withLargerScreenshot } from "../saves";

// Each test uses its own IP address, so the rate limits of one test don't reach another.
let nextIp = 1;
function newIp(): string {
  return `192.0.2.${nextIp++}`;
}

interface Options {
  method?: string;
  path?: string;
  phrase?: string | null;
  body?: Uint8Array;
  ip?: string;
  headers?: Record<string, string>;
}

function request({ method = "GET", path = "/v1/save", phrase = "silt strider", body, ip, headers = {} }: Options) {
  const all: Record<string, string> = { "CF-Connecting-IP": ip ?? newIp(), ...headers };
  if (phrase !== null) all.Authorization = `Phrase ${encodeURIComponent(phrase)}`;
  return exports.default.fetch(new Request(`https://saves.example.workers.dev${path}`, { method, headers: all, body }));
}

const upload = (phrase: string, body: Uint8Array, ip?: string) => request({ method: "PUT", phrase, body, ip });
const download = (phrase: string) => request({ phrase });
const details = (phrase: string) => request({ path: "/v1/save/info", phrase });

// The names of the files in the bucket for a phrase, which is already cleaned up.
async function filesOf(phrase: string): Promise<string[]> {
  const key = await saveKey(phrase);
  const listed = await env.SAVES.list({ prefix: key });
  return listed.objects.map((o) => o.key.slice(key.length)).sort();
}

describe("the Worker", () => {
  it("returns an uploaded save byte for byte", async () => {
    const bytes = save("modern-morrow");
    const put = await upload("download test", bytes);
    expect(put.status).toBe(200);

    const got = await download("download test");
    expect(got.status).toBe(200);
    expect(got.headers.get("Content-Type")).toBe("application/octet-stream");
    expect(new Uint8Array(await got.arrayBuffer())).toEqual(bytes);
  });

  it("returns the details from the upload and from the info request", async () => {
    const before = Date.now();
    const put = await upload("details test", save("modern-morrow"));
    const fromUpload = await put.json<Record<string, unknown>>();
    expect(fromUpload).toMatchObject({
      playerName: "player",
      playerLevel: 1,
      cell: "Balmora, Caius Cosades' House",
      description: "Day 1 - 16 Last Seed 9 a.m.",
      contentFiles: ["builtin.omwscripts", "Morrowind.esm", "Tribunal.esm", "Bloodmoon.esm"],
      formatVersion: 41,
      size: save("modern-morrow").byteLength,
    });
    expect(Date.parse(fromUpload.uploaded as string)).toBeGreaterThanOrEqual(before - 1000);

    const info = await details("details test");
    expect(info.status).toBe(200);
    expect(await info.json()).toEqual(fromUpload);
  });

  it("replaces the save a phrase held", async () => {
    await upload("overwrite test", save("modern-morrow"));
    await upload("overwrite test", save("openmw-folder"));
    expect(new Uint8Array(await (await download("overwrite test")).arrayBuffer())).toEqual(save("openmw-folder"));
    expect((await (await details("overwrite test")).json<{ cell: string }>()).cell).toBe("Seyda Neen");
  });

  it("finds the same save whatever the phrase's case and spacing", async () => {
    await upload("Ghost  Fence", save("modern-morrow"));
    expect((await download("  ghost fence ")).status).toBe(200);
  });

  it("keeps each phrase's save apart", async () => {
    await upload("apart one", save("modern-morrow"));
    await upload("apart two", save("openmw-folder"));
    expect(new Uint8Array(await (await download("apart one")).arrayBuffer())).toEqual(save("modern-morrow"));
  });

  it("answers 404 for a phrase that holds no save", async () => {
    expect((await download("nothing here")).status).toBe(404);
    expect((await details("nothing here")).status).toBe(404);
  });

  it("refuses a file that isn't a current Modern Morrow save", async () => {
    for (const name of ["upstream", "format-43"] as const) {
      const response = await upload(`refused ${name}`, save(name));
      expect(response.status).toBe(400);
      expect(await response.json()).toEqual({ error: "not a save from a current Modern Morrow" });
      expect((await download(`refused ${name}`)).status).toBe(404);
    }
  });

  it("keeps the phrase's save when it refuses an upload", async () => {
    await upload("kept test", save("modern-morrow"));
    const before = await (await details("kept test")).json();
    expect((await upload("kept test", save("upstream"))).status).toBe(400);
    expect(new Uint8Array(await (await download("kept test")).arrayBuffer())).toEqual(save("modern-morrow"));
    expect(await (await details("kept test")).json()).toEqual(before);
  });

  it("leaves one save and its details file for a phrase", async () => {
    await upload("tidy test", save("modern-morrow"));
    await upload("tidy test", save("openmw-folder"));
    await upload("tidy test", save("format-43"));
    const files = await filesOf("tidy test");
    expect(files).toHaveLength(2);
    expect(files[0]).toBe(".json");
    expect(files[1]).toMatch(/^\/[0-9a-f-]{36}$/);
  });

  it("leaves nothing in the bucket for a phrase whose only upload was refused", async () => {
    expect((await upload("refused only", save("upstream"))).status).toBe(400);
    expect(await filesOf("refused only")).toEqual([]);
  });

  it("reads more of a save whose header is longer than the first read", async () => {
    const bytes = withLargerScreenshot(save("modern-morrow"), START_READ_BYTES);
    const put = await upload("long header", bytes);
    expect(put.status).toBe(200);
    expect((await put.json<{ size: number }>()).size).toBe(bytes.byteLength);
    expect(new Uint8Array(await (await download("long header")).arrayBuffer())).toEqual(bytes);
  });

  it("refuses a request without a phrase", async () => {
    expect((await request({ phrase: null })).status).toBe(401);
    expect((await request({ method: "PUT", phrase: null, body: save("modern-morrow") })).status).toBe(401);
    expect((await request({ phrase: "   " })).status).toBe(401);
  });

  it("refuses an upload that says it's too large without reading it", async () => {
    const response = await request({
      method: "PUT",
      body: new Uint8Array(16),
      headers: { "Content-Length": String(MAX_SAVE_BYTES + 1) },
    });
    expect(response.status).toBe(413);
  });

  it("refuses an upload that is too large", async () => {
    const response = await upload("too large", new Uint8Array(MAX_SAVE_BYTES + 1));
    expect(response.status).toBe(413);
  });

  it("answers 404 for any other request", async () => {
    expect((await request({ path: "/" })).status).toBe(404);
    expect((await request({ method: "DELETE" })).status).toBe(404);
    expect((await request({ method: "POST" })).status).toBe(404);
  });

  it("limits uploads from one address to 10 a minute", async () => {
    const ip = newIp();
    const statuses = [];
    for (let i = 0; i < 11; i++) statuses.push((await upload("rate test", save("modern-morrow"), ip)).status);
    expect(statuses.slice(0, 10)).toEqual(Array(10).fill(200));
    expect(statuses[10]).toBe(429);
  });
});
