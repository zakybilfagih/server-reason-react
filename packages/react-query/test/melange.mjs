// Lets the tests load the Melange output, which has no node_modules of its
// own for npm packages: they come from NODE_MODULES, by default
// demo/client/node_modules. Import it before any npm package.
import { register } from "node:module";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const nodeModules = path.resolve(
  process.env.NODE_MODULES ?? path.join(root, "demo/client/node_modules"),
);

register(
  "data:text/javascript," +
    encodeURIComponent(`
      export async function resolve(specifier, context, next) {
        try {
          return await next(specifier, context);
        } catch (error) {
          if (error.code !== \"ERR_MODULE_NOT_FOUND\" || specifier.startsWith(\".\")) throw error;
          return next(specifier, { ...context, parentURL: ${JSON.stringify(
            pathToFileURL(nodeModules + "/").href,
          )} });
        }
      }`),
);

globalThis.self = globalThis;

export const importOutput = (file) =>
  import(
    pathToFileURL(
      path.join(
        root,
        "_build/default/packages/react-query/test/js/packages/react-query/test",
        file,
      ),
    ).href
  );
