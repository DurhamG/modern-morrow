import { describe, expect, it } from "vitest";
import { MAX_DETAILS_BYTES, MAX_TEXT_LENGTH, saveDetails } from "../../src/details";
import type { SaveInfo } from "../../src/savecheck";

const info: SaveInfo = {
  formatVersion: 41,
  playerName: "Lendias",
  playerLevel: 3,
  cell: "Seyda Neen",
  description: "Quicksave",
  contentFiles: ["builtin.omwscripts", "Morrowind.esm"],
};

const uploaded = new Date("2026-11-15T12:00:00Z");

describe("saveDetails", () => {
  it("keeps the save's details with its size and upload time", () => {
    expect(saveDetails(info, 1234, uploaded)).toEqual({
      playerName: "Lendias",
      playerLevel: 3,
      cell: "Seyda Neen",
      description: "Quicksave",
      contentFiles: ["builtin.omwscripts", "Morrowind.esm"],
      formatVersion: 41,
      size: 1234,
      uploaded: "2026-11-15T12:00:00.000Z",
    });
  });

  it("cuts long texts short", () => {
    const long = "x".repeat(MAX_TEXT_LENGTH + 10);
    const details = saveDetails({ ...info, playerName: long, cell: long, description: long, contentFiles: [long] }, 1, uploaded);
    expect(details.playerName).toHaveLength(MAX_TEXT_LENGTH);
    expect(details.cell).toHaveLength(MAX_TEXT_LENGTH);
    expect(details.description).toHaveLength(MAX_TEXT_LENGTH);
    expect(details.contentFiles[0]).toHaveLength(MAX_TEXT_LENGTH);
  });

  it("keeps the first content files of a long load order and says some are missing", () => {
    const contentFiles = Array.from({ length: 500 }, (_, i) => `Some Mod Number ${i}.esp`);
    const details = saveDetails({ ...info, contentFiles }, 1, uploaded);
    expect(new TextEncoder().encode(JSON.stringify(details)).length).toBeLessThanOrEqual(MAX_DETAILS_BYTES);
    expect(details.moreContentFiles).toBe(true);
    expect(details.contentFiles.length).toBeGreaterThan(100);
    expect(details.contentFiles).toEqual(contentFiles.slice(0, details.contentFiles.length));
  });

  it("counts the bytes of characters outside ASCII", () => {
    // Each "é" is two bytes in UTF-8, so these need cutting sooner than ASCII names would.
    const contentFiles = Array.from({ length: 500 }, (_, i) => `${"é".repeat(30)} ${i}.esp`);
    const details = saveDetails({ ...info, contentFiles }, 1, uploaded);
    expect(new TextEncoder().encode(JSON.stringify(details)).length).toBeLessThanOrEqual(MAX_DETAILS_BYTES);
  });

  it("doesn't say content files are missing when none are", () => {
    expect(saveDetails(info, 1, uploaded).moreContentFiles).toBeUndefined();
  });
});
