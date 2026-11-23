// Writes the hash_key.json fixture from TanStack's hashKey:
//   node packages/react-query/test/hash_key.mjs demo/client/node_modules > packages/react-query/native/test/hash_key.json
// Keys are encoded so object fields keep the order they are written in:
// {"list": [...]} is an array and {"obj": [[name, value], ...]} an object.
import { createRequire } from "module";
import path from "path";

const require = createRequire(path.resolve(process.argv[2]) + "/");
const { hashKey } = require("@tanstack/query-core");

const list = (...values) => ({ list: values });
const obj = (...fields) => ({ obj: fields });

const keys = [
  list("note", 1),
  list("note", "1"),
  list("note"),
  list("todos", obj(["page", 2], ["filter", "done"], ["sort", null])),
  list("mixed", obj(["10", "a"], ["2", "b"], ["z", 1], ["a", list(true, false, 1.5)])),
  list("indexes", obj(["4294967295", 1], ["4294967294", 2], ["01", 3], ["-1", 4], ["0", 5], ["b", 6], ["B", 7])),
  list("duplicate", obj(["a", 1], ["b", 2], ["a", 3])),
  list("unicode", obj(["é", 1], ["z", 2], ["ä", 3], ["😀", 4])),
  list("nested", obj(["b", obj(["d", 1], ["c", 2])], ["a", list()]), list(obj())),
  list("text", "quote \" backslash \\ newline \n tab \t control \u0001\u001f\u007f é 😀 \u2028 </script>"),
  list("numbers", 0, -0, -3, 1e21, 1e-7, 0.1, 100, 2 ** 53, 123456789.125, -1.5e-300),
];

const decode = (value) =>
  value === null || typeof value !== "object"
    ? value
    : "list" in value
      ? value.list.map(decode)
      : Object.fromEntries(value.obj.map(([name, field]) => [name, decode(field)]));

console.log(JSON.stringify(keys.map((key) => ({ key, hash: hashKey(decode(key)) })), null, 1));
