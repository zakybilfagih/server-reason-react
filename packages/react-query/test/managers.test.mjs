// Runs ManagersScenario.re in the browser build, where it drives TanStack's
// own focusManager and onlineManager, and checks its log is the one the
// native build writes to managers.json.
//
//   make react-query-check
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { here, importOutput } from "./melange.mjs";

const { run } = await importOutput("ManagersScenario.js");

test("focus and online managers", () => {
  assert.deepEqual(
    run(),
    JSON.parse(readFileSync(path.join(here, "managers.json"), "utf8")),
  );
});
