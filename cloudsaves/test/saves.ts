// The saves in test/saves, which vitest.config.ts reads and hands to the tests.
//
// The harness made them from a new game. modern-morrow is in Modern Morrow's own saves folder, and openmw-folder in
// OpenMW's. upstream was made by upstream OpenMW, and format-43 by an early Modern Morrow build. Neither of those two
// has a UIST record, so the Worker must refuse them.
import { inject } from "vitest";

declare module "vitest" {
  export interface ProvidedContext {
    saves: Record<string, string>;
  }
}

export type SaveName = "modern-morrow" | "openmw-folder" | "upstream" | "format-43";

// A new copy of the save's bytes each time, so a test can change them.
export function save(name: SaveName): Uint8Array {
  const base64 = inject("saves")[name];
  if (base64 === undefined) throw new Error(`no test save called ${name}`);
  return Uint8Array.from(atob(base64), (c) => c.charCodeAt(0));
}

function viewOf(bytes: Uint8Array): DataView {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

// The offset of each record in a save.
export function recordOffsets(bytes: Uint8Array): number[] {
  const view = viewOf(bytes);
  const offsets: number[] = [];
  for (let p = 0; p < bytes.byteLength; p += 16 + view.getUint32(p + 4, true)) offsets.push(p);
  return offsets;
}

// The header's first subrecord is FORM, so its value starts 8 bytes after the record header.
const FORM_VALUE = 24;

export function withFormat(bytes: Uint8Array, format: number): Uint8Array {
  const copy = bytes.slice();
  viewOf(copy).setUint32(FORM_VALUE, format, true);
  return copy;
}

// A copy of the save whose screenshot has extra bytes at its end, which makes its SAVE record longer.
export function withLargerScreenshot(bytes: Uint8Array, extra: number): Uint8Array {
  const view = viewOf(bytes);
  const saveRecord = recordOffsets(bytes)[1];
  const saveEnd = saveRecord + 16 + view.getUint32(saveRecord + 4, true);
  let p = saveRecord + 16;
  while (String.fromCharCode(...bytes.subarray(p, p + 4)) !== "SCRN") p += 8 + view.getUint32(p + 4, true);
  const screenshotEnd = p + 8 + view.getUint32(p + 4, true);

  const copy = new Uint8Array(bytes.byteLength + extra);
  copy.set(bytes.subarray(0, screenshotEnd));
  copy.set(bytes.subarray(screenshotEnd), screenshotEnd + extra);
  const copyView = viewOf(copy);
  copyView.setUint32(p + 4, view.getUint32(p + 4, true) + extra, true);
  copyView.setUint32(saveRecord + 4, saveEnd - saveRecord - 16 + extra, true);
  return copy;
}

// A copy of the save whose last record, its UIST, holds the given number of bytes. A new game's UIST is empty.
export function withUiState(bytes: Uint8Array, size: number, fill = 0): Uint8Array {
  const offsets = recordOffsets(bytes);
  const last = offsets[offsets.length - 1];
  const copy = new Uint8Array(last + 16 + size).fill(fill);
  copy.set(bytes.subarray(0, last + 16));
  viewOf(copy).setUint32(last + 4, size, true);
  return copy;
}
