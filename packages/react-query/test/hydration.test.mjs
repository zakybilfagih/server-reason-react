// Checks that what the native server streams lands in the browser cache
// exactly as TanStack's own dehydrate/hydrate puts it there. Each scenario
// runs twice: once with a TanStack QueryClient on the "server", dehydrated
// and hydrated like TanStack's streaming SSR, and once by feeding
// hydration_stream.json, the native server's output, to hydrateFromServer.
// The scenarios follow query-core's hydration.test.tsx (5.108.2), and the
// boundary ones react-query's HydrationBoundary.test.tsx: a server render is
// a render that never commits, like "should hydrate new but not existing
// queries if transition is aborted".
//
//   make react-query-check
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { here, importOutput } from "./melange.mjs";

const {
  QueryClient,
  QueryClientProvider,
  QueryObserver,
  HydrationBoundary,
  usePrefetchQuery,
  dehydrate,
  hydrate,
  experimental_streamedQuery,
} = await import("@tanstack/react-query");
const { createElement } = await import("react");
const { renderToString } = await import("react-dom/server");
const { hydrateFromServer } = await importOutput("HydrationQueries.js");

const streamed = JSON.parse(
  readFileSync(path.join(here, "hydration_stream.json"), "utf8"),
);

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const settle = () => sleep(30);

const makeClient = () =>
  new QueryClient({
    defaultOptions: {
      queries: { retry: false },
      dehydrate: { shouldDehydrateQuery: () => true },
    },
  });

// A pending query's promise is not JSON: the native server streams its
// result as a later message instead.
const scrub = (value) =>
  JSON.parse(JSON.stringify(value), (name, field) =>
    name === "promise"
      ? undefined
      : name === "dehydratedAt" ||
    ((name === "dataUpdatedAt" || name === "errorUpdatedAt") && field > 0)
      ? "<now>"
      : field,
  );

const snapshot = (client, queryKey) => {
  const query = client.getQueryCache().find({ queryKey, exact: true });
  if (!query) return undefined;
  const { state } = query;
  return {
    data: state.data,
    status: state.status,
    fetchStatus: state.fetchStatus,
    error: state.error?.message ?? null,
    dataUpdateCount: state.dataUpdateCount,
    errorUpdateCount: state.errorUpdateCount,
    fetchFailureCount: state.fetchFailureCount,
    isInvalidated: state.isInvalidated,
    hasDataUpdatedAt: state.dataUpdatedAt > 0,
    queryType: query.queryType,
  };
};

// Puts back the times the fixture scrubbed, as of now.
const stamp = (batch) =>
  JSON.parse(JSON.stringify(batch), (_, field) =>
    field === "<now>" ? Date.now() : field,
  );

// A loader prefetches into its own client, and the server renders its
// dehydrated state in a HydrationBoundary.
const boundary = (name, { cached }) => ({
  queryKey: [name, 1],
  server: async (client) => {
    if (cached) client.setQueryData([name, 1], "cached data");
    await sleep(5);
    const loader = makeClient();
    await loader.prefetchQuery({
      queryKey: [name, 1],
      queryFn: () => "loader data",
    });
    renderToString(
      createElement(
        QueryClientProvider,
        { client },
        createElement(HydrationBoundary, { state: dehydrate(loader) }),
      ),
    );
  },
});

const page = (param) => sleep(10).then(() => `page ${param}`);

const scenarios = {
  resolved: {
    queryKey: ["resolved", 1],
    server: async (client) => {
      await client.prefetchQuery({
        queryKey: ["resolved", 1],
        queryFn: () => "server data",
      });
    },
  },
  pending: {
    queryKey: ["pending", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["pending", 1],
        queryFn: () => sleep(10).then(() => "server data"),
      });
    },
  },
  stale: {
    queryKey: ["stale", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["stale", 1],
        queryFn: () => sleep(10).then(() => "server data"),
      });
    },
    before: (client) =>
      client.setQueryData(["stale", 1], "newer data", {
        updatedAt: Date.now() + 60_000,
      }),
  },
  failing: {
    queryKey: ["failing", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["failing", 1],
        queryFn: () => sleep(10).then(() => Promise.reject(new Error("secret"))),
      });
    },
  },
  retried: {
    queryKey: ["retried", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["retried", 1],
        queryFn: () => sleep(10).then(() => Promise.reject(new Error("secret"))),
      });
    },
    // A component mounts after the server's error and fetches the query.
    after: async (client) => {
      new QueryObserver(client, {
        queryKey: ["retried", 1],
        queryFn: () => "client data",
      }).subscribe(() => {});
      await settle();
    },
  },
  infinite: {
    queryKey: ["infinite"],
    infinite: true,
    server: (client) => {
      client.prefetchInfiniteQuery({
        queryKey: ["infinite"],
        queryFn: ({ pageParam }) => page(pageParam),
        initialPageParam: 0,
        getNextPageParam: (_, __, last) => last + 1,
        pages: 2,
      });
    },
    // A refetch fetches every page the server sent again.
    after: async (client) => {
      await client.infiniteQuery({
        queryKey: ["infinite"],
        queryFn: ({ pageParam }) => sleep(5).then(() => `client page ${pageParam}`),
        initialPageParam: 0,
        getNextPageParam: (_, __, last) => last + 1,
        staleTime: 0,
      });
    },
  },
  boundary: boundary("boundary", { cached: false }),
  existing: boundary("existing", { cached: true }),
  // Newer data hydrated into a query whose fetch is running: the shell
  // carries it, and the fetch's result only replaces it on the server.
  // A prefetch nothing renders, still running when the render ends: the
  // response stays open for its result.
  outlives: {
    queryKey: ["outlives", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["outlives", 1],
        queryFn: () => sleep(10).then(() => "server data"),
      });
    },
  },
  // A streamed query: each chunk updates the cache, and the result is all
  // of them.
  streamed: {
    queryKey: ["streamed", 1],
    server: (client) => {
      client.prefetchQuery({
        queryKey: ["streamed", 1],
        queryFn: experimental_streamedQuery({
          streamFn: async function* () {
            for (const chunk of ["a", "b", "c"]) {
              await sleep(5);
              yield chunk;
            }
          },
          reducer: (data, chunk) => data + chunk,
          initialValue: "",
        }),
      });
    },
  },
  // usePrefetchQuery in a component the server renders.
  "render-prefetch": {
    queryKey: ["render-prefetch", 1],
    server: (client) => {
      const Prefetch = () => {
        usePrefetchQuery({
          queryKey: ["render-prefetch", 1],
          queryFn: () => sleep(10).then(() => "server data"),
        });
        return null;
      };
      renderToString(
        createElement(QueryClientProvider, { client }, createElement(Prefetch)),
      );
    },
  },
  "in-flight": {
    queryKey: ["in-flight", 1],
    server: async (client) => {
      client.prefetchQuery({
        queryKey: ["in-flight", 1],
        queryFn: () => sleep(10).then(() => "server data"),
      });
      const loader = makeClient();
      await loader.prefetchQuery({
        queryKey: ["in-flight", 1],
        queryFn: () => "loader data",
      });
      hydrate(client, dehydrate(loader));
    },
  },
};

// TanStack's streaming SSR: the server dehydrates its pending queries with
// their promises, and the browser hydrates them.
const reference = async (scenario) => {
  const server = makeClient();
  await scenario.server(server);
  const dehydrated = dehydrate(server);
  const browser = makeClient();
  scenario.before?.(browser);
  hydrate(browser, dehydrated);
  const shell = snapshot(browser, scenario.queryKey);
  await settle();
  const settled = snapshot(browser, scenario.queryKey);
  await scenario.after?.(browser);
  return {
    dehydrated: scrub(dehydrated.queries),
    final: scrub(dehydrate(server).queries),
    shell,
    settled,
    after: snapshot(browser, scenario.queryKey),
  };
};

const native = async (name, scenario) => {
  const browser = makeClient();
  scenario.before?.(browser);
  delete globalThis.__RQ;
  for (const batch of streamed[name].shell) {
    (self.__RQ = self.__RQ || []).push(stamp(batch));
  }
  hydrateFromServer(browser);
  const shell = snapshot(browser, scenario.queryKey);
  await sleep(10);
  for (const batch of streamed[name].later) self.__RQ.push(stamp(batch));
  await sleep(20);
  const settled = snapshot(browser, scenario.queryKey);
  await scenario.after?.(browser);
  return { shell, settled, after: snapshot(browser, scenario.queryKey) };
};

for (const [name, scenario] of Object.entries(scenarios)) {
  test(name, async () => {
    const expected = await reference(scenario);
    const actual = await native(name, scenario);
    assert.deepEqual(
      streamed[name].shell.flat(),
      expected.dehydrated,
      "the shell has what dehydrate gives",
    );
    const success = streamed[name].later
      .flat()
      .filter((message) => message.state.status === "success");
    if (success.length > 0) {
      assert.deepEqual(success, expected.final, "a result is what dehydrate gives");
    }
    assert.deepEqual(actual.shell, expected.shell, "after the shell");
    assert.deepEqual(actual.settled, expected.settled, "after the stream");
    assert.deepEqual(actual.after, expected.after, "after the browser takes over");
  });
}
