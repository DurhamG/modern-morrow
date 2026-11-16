// Checks that an upload is a save from a current Modern Morrow build, and reads the details the game shows for it.
//
// A save is an ESM3 file, little-endian, as components/esm3/esmwriter.cpp writes it:
//   record    = tag[4] | u32 dataSize | u32 unused | u32 flags | subrecords
//   subrecord = tag[4] | u32 dataSize | data
// Its records are, in order:
//   TES3, the header: FORM (the format version), HEDR, then a MAST and DATA pair for each content file.
//   SAVE, the save's details: PLNA (player name), PLLE (level), PLCE (cell), DESC (description), SCRN (screenshot)
//   and others.
//   The game's state, in many records.
//   UIST, what Modern Morrow's controller menus remember. Modern Morrow writes it last in every save.
//
// Reading a whole 15 MB save costs more CPU time than the free Workers plan allows a request. So the Worker stores
// the upload without reading it, then reads back only its start and its end:
//   - The start holds the TES3 and SAVE records. checkStart reads every subrecord in them.
//   - The end must be a whole UIST record that ends exactly where the file ends. checkEnd looks for it.
// The game's state in between isn't read, so a file with a valid start and end but anything in between passes. That
// costs little, because anything could already be hidden in a valid save's screenshot.

export interface SaveInfo {
  formatVersion: number;
  playerName: string;
  playerLevel?: number;
  cell?: string;
  description?: string;
  contentFiles: string[];
}

export class InvalidSave extends Error {}

export const MAX_SAVE_BYTES = 20 * 1024 * 1024;
export const MIN_SAVE_BYTES = 64;

// How much of the start and of the end the Worker reads first. The TES3 and SAVE records took 58 to 127 KB in the
// saves tried, most of it the screenshot, so most saves need one read of the start. A long content file list can
// make them longer, up to MAX_START_BYTES.
export const START_READ_BYTES = 256 * 1024;
export const MAX_START_BYTES = 2 * 1024 * 1024;
// A UIST record holds the pinned and archived quests' names, so it is far smaller than this.
export const END_READ_BYTES = 64 * 1024;

// 37 is the format of OpenMW 0.51.0, the newest release when cloud saves were added. Modern Morrow writes OpenMW's
// current format, which was 41 then. The upper limit leaves room for later formats.
export const MIN_FORMAT = 37;
export const MAX_FORMAT = 200;

const RECORD_HEADER = 16;
const SUBRECORD_HEADER = 8;
const NOT_CURRENT = "not a save from a current Modern Morrow";

// Tags are four upper case letters, digits or underscores, such as NPC_ and CAM_.
const TAG_BYTE = new Uint8Array(256);
for (let c = 0; c < 256; c++) TAG_BYTE[c] = +((c >= 65 && c <= 90) || (c >= 48 && c <= 57) || c === 95);

function isTag(bytes: Uint8Array, offset: number): boolean {
  return !!(TAG_BYTE[bytes[offset]] & TAG_BYTE[bytes[offset + 1]] & TAG_BYTE[bytes[offset + 2]] & TAG_BYTE[bytes[offset + 3]]);
}

function tagAt(bytes: Uint8Array, offset: number): string {
  if (!isTag(bytes, offset)) throw new InvalidSave(`bad tag at byte ${offset}`);
  return String.fromCharCode(bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]);
}

function viewOf(bytes: Uint8Array): DataView {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

// Saves store strings in the game's legacy 8-bit encoding, which depends on the language of the player's Morrowind,
// such as windows-1252 for English or windows-1251 for Russian. The Worker can't know which one it is. So each byte
// becomes the character with the same number, and the game turns the characters back into bytes to decode them.
function legacyString(bytes: Uint8Array, offset: number, size: number): string {
  let end = offset + size;
  while (end > offset && bytes[end - 1] === 0) end--;
  let text = "";
  for (let i = offset; i < end; i++) text += String.fromCharCode(bytes[i]);
  return text;
}

interface Subrecord {
  tag: string;
  offset: number;
  size: number;
}

// Reads the subrecords of a record, which must fill it exactly.
function subrecords(bytes: Uint8Array, view: DataView, start: number, end: number): Subrecord[] {
  const result: Subrecord[] = [];
  let p = start;
  while (p < end) {
    if (p + SUBRECORD_HEADER > end) throw new InvalidSave(`subrecord header cut short at byte ${p}`);
    const tag = tagAt(bytes, p);
    const size = view.getUint32(p + 4, true);
    const offset = p + SUBRECORD_HEADER;
    if (offset + size > end) throw new InvalidSave(`subrecord ${tag} runs past its record at byte ${p}`);
    result.push({ tag, offset, size });
    p = offset + size;
  }
  return result;
}

// From format 23, HEDR holds an f32 version and an i32 file type, then the author and the description, each a u32
// length and that many bytes, then an i32 record count.
function checkHedr(view: DataView, hedr: Subrecord): void {
  const end = hedr.offset + hedr.size;
  let p = hedr.offset + 8;
  for (let i = 0; i < 2; i++) {
    if (p + 4 > end) throw new InvalidSave("bad HEDR");
    p += 4 + view.getUint32(p, true);
  }
  if (p + 4 !== end) throw new InvalidSave("bad HEDR");
}

function readHeader(bytes: Uint8Array, view: DataView, subs: Subrecord[], info: SaveInfo): void {
  const form = subs[0];
  if (form?.tag !== "FORM" || form.size !== 4) throw new InvalidSave("the header has no format version");
  info.formatVersion = view.getUint32(form.offset, true);
  if (info.formatVersion < MIN_FORMAT || info.formatVersion > MAX_FORMAT)
    throw new InvalidSave(`format ${info.formatVersion} isn't supported`);

  const hedr = subs[1];
  if (hedr?.tag !== "HEDR") throw new InvalidSave("bad HEDR");
  checkHedr(view, hedr);

  for (let i = 2; i < subs.length; i += 2) {
    const mast = subs[i];
    const data = subs[i + 1];
    if (mast.tag !== "MAST" || data?.tag !== "DATA" || data.size !== 8) throw new InvalidSave("bad content file list");
    info.contentFiles.push(legacyString(bytes, mast.offset, mast.size));
  }
}

function readSaveRecord(bytes: Uint8Array, view: DataView, subs: Subrecord[], info: SaveInfo): void {
  if (subs[0]?.tag !== "PLNA") throw new InvalidSave("the save has no player name");
  if (!subs.some((s) => s.tag === "SCRN")) throw new InvalidSave("the save has no screenshot");
  for (const s of subs) {
    if (s.tag === "PLNA") info.playerName = legacyString(bytes, s.offset, s.size);
    else if (s.tag === "PLLE" && s.size === 4) info.playerLevel = view.getInt32(s.offset, true);
    else if (s.tag === "PLCE") info.cell = legacyString(bytes, s.offset, s.size);
    else if (s.tag === "DESC") info.description = legacyString(bytes, s.offset, s.size);
  }
}

// The end of a record that starts at offset, after checking its tag and that it ends within limit.
function recordEnd(bytes: Uint8Array, offset: number, limit: number): number {
  if (offset + RECORD_HEADER > limit) throw new InvalidSave(`record header cut short at byte ${offset}`);
  const tag = tagAt(bytes, offset);
  const end = offset + RECORD_HEADER + viewOf(bytes).getUint32(offset + 4, true);
  if (end > limit) throw new InvalidSave(`record ${tag} runs past the end of the file at byte ${offset}`);
  return end;
}

// How many bytes from the start of the file hold its TES3 and SAVE records. start is the first bytes the Worker
// read, which must hold at least both records' headers, and fileSize is the whole file's size.
export function startLength(start: Uint8Array, fileSize: number): number {
  if (start.byteLength < RECORD_HEADER) throw new InvalidSave("the file is too small");
  if (tagAt(start, 0) !== "TES3") throw new InvalidSave("the file doesn't start with a TES3 header");
  const view = viewOf(start);
  const saveStart = RECORD_HEADER + view.getUint32(4, true);
  if (saveStart + RECORD_HEADER > Math.min(start.byteLength, fileSize))
    throw new InvalidSave("the header isn't followed by a SAVE record");
  if (tagAt(start, saveStart) !== "SAVE") throw new InvalidSave("the header isn't followed by a SAVE record");
  const length = saveStart + RECORD_HEADER + view.getUint32(saveStart + 4, true);
  if (length > MAX_START_BYTES) throw new InvalidSave("the save's header is too large");
  return length;
}

// Reads the TES3 and SAVE records, which start must hold whole, and returns the save's details.
export function checkStart(start: Uint8Array, fileSize: number): SaveInfo {
  if (fileSize > MAX_SAVE_BYTES) throw new InvalidSave("the file is too large");
  if (fileSize < MIN_SAVE_BYTES) throw new InvalidSave("the file is too small");
  const limit = Math.min(start.byteLength, fileSize);
  const view = viewOf(start);
  const info: SaveInfo = { formatVersion: 0, playerName: "", contentFiles: [] };

  const headerEnd = recordEnd(start, 0, limit);
  if (tagAt(start, 0) !== "TES3") throw new InvalidSave("the file doesn't start with a TES3 header");
  readHeader(start, view, subrecords(start, view, RECORD_HEADER, headerEnd), info);

  const saveEnd = recordEnd(start, headerEnd, limit);
  if (tagAt(start, headerEnd) !== "SAVE") throw new InvalidSave("the header isn't followed by a SAVE record");
  readSaveRecord(start, view, subrecords(start, view, headerEnd + RECORD_HEADER, saveEnd), info);

  if (saveEnd >= fileSize) throw new InvalidSave("the save has no game state");
  return info;
}

// Checks that end, the last bytes of the file, finishes with a whole UIST record. It looks back from the end for a
// UIST tag whose record would end exactly where the file does.
export function checkEnd(end: Uint8Array): void {
  const view = viewOf(end);
  for (let p = end.byteLength - RECORD_HEADER; p >= 0; p--) {
    if (end[p] === 85 && isTag(end, p) && tagAt(end, p) === "UIST" && p + RECORD_HEADER + view.getUint32(p + 4, true) === end.byteLength)
      return;
  }
  throw new InvalidSave(NOT_CURRENT);
}

// Checks a whole save the way the Worker checks a stored one, from its start and its end.
export function checkSaveFile(bytes: Uint8Array): SaveInfo {
  if (bytes.byteLength > MAX_SAVE_BYTES) throw new InvalidSave("the file is too large");
  if (bytes.byteLength < MIN_SAVE_BYTES) throw new InvalidSave("the file is too small");
  const info = checkStart(bytes.subarray(0, startLength(bytes, bytes.byteLength)), bytes.byteLength);
  checkEnd(bytes.subarray(Math.max(0, bytes.byteLength - END_READ_BYTES)));
  return info;
}
