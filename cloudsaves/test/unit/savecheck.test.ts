import { describe, expect, it } from "vitest";
import {
  checkEnd,
  checkSaveFile,
  END_READ_BYTES,
  InvalidSave,
  MAX_SAVE_BYTES,
  MAX_START_BYTES,
  START_READ_BYTES,
  startLength,
} from "../../src/savecheck";
import { recordOffsets, save, withFormat, withLargerScreenshot, withUiState } from "../saves";

// HEDR follows FORM. Its data starts with an f32 and an i32, then the author's length.
const HEDR_AUTHOR_LENGTH = 28 + 8 + 8;
const NOT_CURRENT = "not a save from a current Modern Morrow";

function rejection(check: () => unknown): string {
  try {
    check();
  } catch (error) {
    if (error instanceof InvalidSave) return error.message;
    throw error;
  }
  throw new Error("the save check accepted it");
}

const refusal = (bytes: Uint8Array) => rejection(() => checkSaveFile(bytes));

describe("checkSaveFile", () => {
  it("accepts a save in Modern Morrow's folder and reads its details", () => {
    expect(checkSaveFile(save("modern-morrow"))).toEqual({
      formatVersion: 41,
      playerName: "player",
      playerLevel: 1,
      cell: "Balmora, Caius Cosades' House",
      description: "Day 1 - 16 Last Seed 9 a.m.",
      contentFiles: ["builtin.omwscripts", "Morrowind.esm", "Tribunal.esm", "Bloodmoon.esm"],
    });
  });

  it("accepts a save that Modern Morrow wrote in OpenMW's folder", () => {
    expect(checkSaveFile(save("openmw-folder")).cell).toBe("Seyda Neen");
  });

  it("refuses a save from upstream OpenMW, which has no UIST record", () => {
    expect(refusal(save("upstream"))).toBe(NOT_CURRENT);
  });

  it("refuses the old formats 42 and 43 that early Modern Morrow builds wrote", () => {
    expect(refusal(save("format-43"))).toBe(NOT_CURRENT);
    expect(refusal(withFormat(save("format-43"), 42))).toBe(NOT_CURRENT);
  });

  it("accepts formats from 37, OpenMW 0.51's, and leaves room for later ones", () => {
    expect(checkSaveFile(withFormat(save("modern-morrow"), 37)).formatVersion).toBe(37);
    expect(checkSaveFile(withFormat(save("modern-morrow"), 42)).formatVersion).toBe(42);
    expect(refusal(withFormat(save("modern-morrow"), 36))).toBe("format 36 isn't supported");
    expect(refusal(withFormat(save("modern-morrow"), 201))).toBe("format 201 isn't supported");
  });

  it("keeps each byte of a legacy string as the character with the same number", () => {
    // "player" becomes "pläyer" in windows-1252, where ä is byte 0xE4. PLNA is the SAVE record's first subrecord.
    const bytes = save("modern-morrow");
    const plna = recordOffsets(bytes)[1] + 16;
    bytes[plna + 8 + 2] = 0xe4;
    expect(checkSaveFile(bytes).playerName).toBe("pläyer");
  });

  it("refuses a save cut short", () => {
    const bytes = save("modern-morrow");
    const offsets = recordOffsets(bytes);
    expect(refusal(bytes.subarray(0, bytes.length - 7))).toBe(NOT_CURRENT);
    expect(refusal(bytes.subarray(0, offsets[offsets.length - 1]))).toBe(NOT_CURRENT);
    // Inside the SAVE record, so its end is past the end of the file.
    expect(refusal(bytes.subarray(0, offsets[2] - 100))).toMatch(/^record SAVE runs past the end of the file/);
  });

  it("refuses a save with bytes added at the end", () => {
    const bytes = save("modern-morrow");
    for (const extra of [5, 16, 1000]) {
      const longer = new Uint8Array(bytes.length + extra);
      longer.set(bytes);
      expect(refusal(longer)).toBe(NOT_CURRENT);
    }
  });

  it("accepts a UIST record that holds data", () => {
    expect(checkSaveFile(withUiState(save("modern-morrow"), 300)).playerName).toBe("player");
  });

  it("refuses a UIST record whose size doesn't reach the end of the file", () => {
    const bytes = withUiState(save("modern-morrow"), 300);
    const last = recordOffsets(bytes).at(-1)!;
    new DataView(bytes.buffer).setUint32(last + 4, 299, true);
    expect(refusal(bytes)).toBe(NOT_CURRENT);
  });

  it("doesn't read the game state between the SAVE and UIST records", () => {
    // The Worker can't afford to read a large save whole. See src/savecheck.ts.
    const bytes = save("modern-morrow");
    bytes[recordOffsets(bytes)[5]] = "n".charCodeAt(0);
    expect(checkSaveFile(bytes).playerName).toBe("player");
  });

  it("refuses a HEDR whose strings don't fill it", () => {
    const bytes = save("modern-morrow");
    new DataView(bytes.buffer).setUint32(HEDR_AUTHOR_LENGTH, 1000, true);
    expect(refusal(bytes)).toBe("bad HEDR");
  });

  it("refuses a file that doesn't start with a TES3 header and a SAVE record", () => {
    const bytes = save("modern-morrow");
    bytes[0] = "X".charCodeAt(0);
    expect(refusal(bytes)).toBe("the file doesn't start with a TES3 header");

    const noSave = save("modern-morrow");
    noSave[recordOffsets(noSave)[1]] = "X".charCodeAt(0);
    expect(refusal(noSave)).toBe("the header isn't followed by a SAVE record");
  });

  it("refuses a save with nothing after its SAVE record", () => {
    const bytes = save("modern-morrow");
    expect(refusal(bytes.subarray(0, recordOffsets(bytes)[2]))).toBe("the save has no game state");
  });

  it("refuses a zip and random bytes", () => {
    const zip = new Uint8Array(1000);
    zip.set([0x50, 0x4b, 0x03, 0x04]);
    expect(refusal(zip)).toMatch(/^bad tag/);

    const random = new Uint8Array(64 * 1024);
    let seed = 12345;
    for (let i = 0; i < random.length; i++) {
      seed = (seed * 1103515245 + 12345) >>> 0;
      random[i] = seed >>> 24;
    }
    expect(() => checkSaveFile(random)).toThrow(InvalidSave);
  });

  it("refuses files that are too small or too large", () => {
    expect(refusal(new Uint8Array(10))).toBe("the file is too small");
    expect(refusal(new Uint8Array(MAX_SAVE_BYTES + 1))).toBe("the file is too large");
  });
});

describe("startLength", () => {
  it("is where the SAVE record ends", () => {
    const bytes = save("modern-morrow");
    expect(startLength(bytes.subarray(0, START_READ_BYTES), bytes.length)).toBe(recordOffsets(bytes)[2]);
  });

  it("can be longer than the first read, which the Worker then reads again", () => {
    const bytes = withLargerScreenshot(save("modern-morrow"), START_READ_BYTES);
    const length = startLength(bytes.subarray(0, START_READ_BYTES), bytes.length);
    expect(length).toBeGreaterThan(START_READ_BYTES);
    expect(length).toBe(recordOffsets(bytes)[2]);
    expect(checkSaveFile(bytes).playerName).toBe("player");
  });

  it("refuses a start longer than the limit", () => {
    const bytes = withLargerScreenshot(save("modern-morrow"), MAX_START_BYTES);
    expect(rejection(() => startLength(bytes.subarray(0, START_READ_BYTES), bytes.length))).toBe(
      "the save's header is too large",
    );
  });
});

describe("checkEnd", () => {
  it("finds a UIST record anywhere in the last bytes read", () => {
    const bytes = withUiState(save("modern-morrow"), END_READ_BYTES - 100);
    expect(() => checkEnd(bytes.subarray(bytes.length - END_READ_BYTES))).not.toThrow();
  });

  it("refuses a UIST record that starts before the last bytes read", () => {
    const bytes = withUiState(save("modern-morrow"), END_READ_BYTES);
    expect(rejection(() => checkEnd(bytes.subarray(bytes.length - END_READ_BYTES)))).toBe(NOT_CURRENT);
  });

  it("isn't fooled by a UIST tag in the data that doesn't end where the file does", () => {
    // A UIST record whose data holds the bytes of another UIST header, with a size that doesn't reach the end.
    const bytes = withUiState(save("modern-morrow"), 64);
    const last = recordOffsets(bytes).at(-1)!;
    bytes.set([0x55, 0x49, 0x53, 0x54, 10, 0, 0, 0], last + 16 + 8);
    expect(() => checkEnd(bytes.subarray(bytes.length - 200))).not.toThrow();
    new DataView(bytes.buffer).setUint32(last + 4, 63, true);
    expect(rejection(() => checkEnd(bytes.subarray(bytes.length - 200)))).toBe(NOT_CURRENT);
  });
});
