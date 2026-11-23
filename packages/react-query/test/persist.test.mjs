// Runs PersistScenario.re in the browser build, where it saves and restores
// through TanStack's persist-client functions, and checks its log is the one
// the native build writes to persist.json.
//
//   make react-query-check
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { here, importOutput } from "./melange.mjs";

const { run } = await importOutput("PersistScenario.js");

test("persist and restore", async () => {
  assert.deepEqual(
    await run(),
    JSON.parse(readFileSync(path.join(here, "persist.json"), "utf8")),
  );
});
