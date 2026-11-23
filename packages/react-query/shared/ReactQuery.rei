/** Query keys. A query's key is its name followed by its parts, and two keys
    are the same query when their [hash] is: JSON with sorted object fields,
    like TanStack's [hashKey]. */
module Key: {
  type t;
  let string: string => t;
  let int: int => t;
  let float: float => t;
  let bool: bool => t;
  let null: t;
  let list: list(t) => t;
  let obj: list((string, t)) => t;
  let hash: list(t) => string;
};

/** How a query's data crosses from the server to the browser. [decode] raises
    on malformed JSON, like the functions [[@deriving json]] generates; the
    browser then fetches the query itself. */
module Codec: {
  type t('data);
  let make:
    (~encode: 'data => Melange_json.t, ~decode: Melange_json.t => 'data) =>
    t('data);
};

module StaleTime: {
  type t =
    | Ms(float)
    | Infinite
    /* Never stale, not even for an explicit invalidation. */
    | Static;
};

module Retry: {
  type t =
    | Never
    | Times(int)
    /* Gets the number of failures so far and the last error. */
    | When((int, exn) => bool);
};

module NetworkMode: {
  type t =
    | Online
    | Always
    | OfflineFirst;
};

module Query: {
  type t('key, 'data);

  /** [name] is the first element of every key of this query and must be
      unique. [fetch] runs on the server and in the browser. */
  let make:
    (
      ~name: string,
      ~key: 'key => list(Key.t),
      ~codec: Codec.t('data),
      ~fetch: (~signal: Fetch.signal, 'key) => Js.Promise.t('data),
      ~staleTime: StaleTime.t=?,
      ~gcTime: float=?,
      ~retry: Retry.t=?,
      ~retryDelay: (int, exn) => float=?,
      ~networkMode: NetworkMode.t=?,
      unit
    ) =>
    t('key, 'data);

  /** The hash of the query's key for [key], like TanStack's [hashKey]: the
      [CachedQuery.hash] of its entry in a client. */
  let hash: (t('key, _), 'key) => string;
};

/** The pages of an infinite query fetched so far, in order, and the param
    each one was fetched with. */
/** Chunks pulled one at a time, for [StreamedQuery]. */
module Stream: {
  type t('chunk);

  /** [next] gives each chunk in turn, then [None]. */
  let make: (unit => Js.Promise.t(option('chunk))) => t('chunk);
  let fromArray: array('chunk) => t('chunk);
};

/** TanStack's [experimental_streamedQuery]: a query whose data arrives in
    chunks. Each chunk goes into the cache through [reducer] as it arrives,
    so [useQuery] shows the data growing, and the query settles when the
    stream ends. [useSuspenseQuery] suspends only while the query has no
    data: until the stream ends when nothing has arrived yet, and not at all
    once a chunk has, so it can show part of the data, also on the server.
    For TanStack's default, a list of every chunk, use
    [~reducer=(chunks, chunk) => Array.append(chunks, [|chunk|])] and
    [~initialValue=[||]]. */
module StreamedQuery: {
  /** What a refetch does with the data: [Reset] (the default) starts again
      from [initialValue], [Append] adds to it, and [Replace] keeps it until
      the new stream ends. The server never refetches. */
  type refetchMode =
    | Reset
    | Append
    | Replace;

  let make:
    (
      ~name: string,
      ~key: 'key => list(Key.t),
      ~codec: Codec.t('data),
      ~stream:
        (~signal: Fetch.signal, 'key) => Js.Promise.t(Stream.t('chunk)),
      ~reducer: ('data, 'chunk) => 'data,
      ~initialValue: 'data,
      ~refetchMode: refetchMode=?,
      ~staleTime: StaleTime.t=?,
      ~gcTime: float=?,
      ~retry: Retry.t=?,
      ~retryDelay: (int, exn) => float=?,
      ~networkMode: NetworkMode.t=?,
      unit
    ) =>
    Query.t('key, 'data);
};

type infiniteData('page, 'param) = {
  pages: array('page),
  pageParams: array('param),
};

module InfiniteQuery: {
  type t('key, 'page, 'param);

  /** A query fetched one page at a time. [getNextPageParam] gets the pages
      so far and returns the next page's param, or [None] when there is no
      next page; [getPreviousPageParam] does the same backwards. Keeps at
      most [maxPages] pages. */
  let make:
    (
      ~name: string,
      ~key: 'key => list(Key.t),
      ~pageCodec: Codec.t('page),
      ~paramCodec: Codec.t('param),
      ~initialPageParam: 'param,
      ~fetchPage:
        (~signal: Fetch.signal, 'key, 'param) => Js.Promise.t('page),
      ~getNextPageParam: infiniteData('page, 'param) => option('param),
      ~getPreviousPageParam: infiniteData('page, 'param) => option('param)=?,
      ~maxPages: int=?,
      ~staleTime: StaleTime.t=?,
      ~gcTime: float=?,
      ~retry: Retry.t=?,
      ~retryDelay: (int, exn) => float=?,
      ~networkMode: NetworkMode.t=?,
      unit
    ) =>
    t('key, 'page, 'param);
};

type status('data) =
  | Pending
  | Error(exn)
  | Success('data);

type fetchStatus =
  | Fetching
  | Paused
  | Idle;

/** Which queries a cache operation applies to. */
module Filter: {
  type t;
  let exact: (Query.t('key, _), 'key) => t;
  /** Every key of the query. */
  let query: Query.t(_, _) => t;
  let exactInfinite: (InfiniteQuery.t('key, _, _), 'key) => t;
  let infinite: InfiniteQuery.t(_, _, _) => t;
  let all: t;
  /** Narrows a filter. [~active] keeps the queries a mounted component
      reads ([true]) or no component reads ([false]). */
  let where:
    (~active: bool=?, ~stale: bool=?, ~fetchStatus: fetchStatus=?, t) => t;
};

/** Which of the invalidated queries refetch at once. */
module Refetch: {
  type t =
    | Active
    | Inactive
    | All
    | Never;
};

/** A query's state in the cache. */
type queryState('data) = {
  data: option('data),
  error: option(exn),
  dataUpdatedAt: float,
  errorUpdatedAt: float,
  fetchStatus,
  isInvalidated: bool,
};

/** The query a cache event is about. Its data has no type here: any query
    can be the one. */
module CachedQuery: {
  type t;
  let name: t => string;
  let hash: t => string;
  let hasData: t => bool;
};

/** The mutation a cache event is about. */
module CachedMutation: {
  type t;
  let name: t => string;
};

module QueryClient: {
  type t;

  /** Defaults for every query of this client. [onQueryError] and
      [onMutationError] see every failure in the client, once its retries
      are spent; on the server they never run, since a query that fails
      there runs again in the browser. */
  let make:
    (
      ~staleTime: StaleTime.t=?,
      ~gcTime: float=?,
      ~retry: Retry.t=?,
      ~retryDelay: (int, exn) => float=?,
      ~networkMode: NetworkMode.t=?,
      ~refetchOnWindowFocus: bool=?,
      ~refetchOnReconnect: bool=?,
      ~onQueryError: (exn, CachedQuery.t) => unit=?,
      ~onMutationError: (exn, CachedMutation.t) => unit=?,
      unit
    ) =>
    t;

  /** Every query in the client's cache, like TanStack's
      [getQueryCache().getAll()]. */
  let cachedQueries: t => array(CachedQuery.t);

  /** Starts the query unless its data is fresh. The promise resolves when it
      settles and never rejects. On the server, the browser waits for the
      server's result instead of fetching the query itself. */
  let prefetch: (t, Query.t('key, _), 'key) => Js.Promise.t(unit);

  /** Like [prefetch], but resolves with the data and rejects with the
      error. */
  let fetch: (t, Query.t('key, 'data), 'key) => Js.Promise.t('data);

  /** The cached data, even when stale, or else [fetch]. */
  let ensure: (t, Query.t('key, 'data), 'key) => Js.Promise.t('data);

  /** [prefetch] for an infinite query: fetches its first [pages] pages, one
      by default, unless it has fresh data. */
  let prefetchInfinite:
    (t, ~pages: int=?, InfiniteQuery.t('key, _, _), 'key) =>
    Js.Promise.t(unit);

  /** [fetch] for an infinite query. */
  let fetchInfinite:
    (t, ~pages: int=?, InfiniteQuery.t('key, 'page, 'param), 'key) =>
    Js.Promise.t(infiniteData('page, 'param));

  /** Marks the matching queries stale and refetches the ones [refetch]
      selects, by default the active ones. Does nothing on the server. */
  let invalidate: (t, ~refetch: Refetch.t=?, Filter.t) => Js.Promise.t(unit);

  /** Refetches the matching queries, fresh or not. Does nothing on the
      server. */
  let refetch: (t, Filter.t) => Js.Promise.t(unit);

  /** Cancels the matching queries' fetches, for instance before an optimistic
      update so a fetch that started earlier does not overwrite it. Does
      nothing on the server. */
  let cancel: (t, Filter.t) => Js.Promise.t(unit);

  /** Drops the matching queries from the cache. Does nothing on the
      server. */
  let remove: (t, Filter.t) => unit;

  /** Puts the matching queries back in their initial state and refetches
      the active ones. Does nothing on the server. */
  let reset: (t, Filter.t) => Js.Promise.t(unit);

  /** Drops every query and mutation. Does nothing on the server. */
  let clear: t => unit;

  /** How many matching queries are fetching. Zero on the server. */
  let isFetching: (t, Filter.t) => int;

  /** The query's data in the cache, without fetching. */
  let getQueryData: (t, Query.t('key, 'data), 'key) => option('data);

  /** The data of every key of the query, in no particular order. */
  let getQueriesData: (t, Query.t(_, 'data)) => array('data);

  let getQueryState:
    (t, Query.t('key, 'data), 'key) => option(queryState('data));

  /** Replaces the query's data with what [update] returns from the current
      data. [None] leaves it unchanged. On the server, the new data streams
      to the browser. */
  let setQueryData:
    (t, Query.t('key, 'data), 'key, option('data) => option('data)) => unit;

  let getInfiniteQueryData:
    (t, InfiniteQuery.t('key, 'page, 'param), 'key) =>
    option(infiniteData('page, 'param));

  /** [setQueryData] for an infinite query. */
  let setInfiniteQueryData:
    (
      t,
      InfiniteQuery.t('key, 'page, 'param),
      'key,
      option(infiniteData('page, 'param)) =>
      option(infiniteData('page, 'param))
    ) =>
    unit;

  /** [setQueryData] for every key of the query in the cache. */
  let setQueriesData:
    (t, Query.t(_, 'data), option('data) => option('data)) => unit;

  module Provider: {
    [@react.component]
    let make: (~client: t, ~children: React.element) => React.element;
  };
};

let useQueryClient: unit => QueryClient.t;

/** How many matching queries are fetching. Zero on the server and while
    hydrating, so the browser hydrates the server's markup. */
let useIsFetching: (~filter: Filter.t=?, unit) => int;

/** Read with accessors, so a component re-renders only when what it reads
    changes. */
module QueryResult: {
  type t('data);
  let status: t('data) => status('data);
  /** The last data, also while a refetch failed. */
  let data: t('data) => option('data);
  let error: t(_) => option(exn);
  let fetchStatus: t(_) => fetchStatus;
  let isFetching: t(_) => bool;
  let isStale: t(_) => bool;
  let dataUpdatedAt: t(_) => float;
  let refetch: t(_) => Js.Promise.t(unit);
};

module SuspenseResult: {
  type t('data);
  let data: t('data) => 'data;
  /** The error of the last refetch, while [data] keeps the previous result. */
  let refetchError: t(_) => option(exn);
  let fetchStatus: t(_) => fetchStatus;
  let isFetching: t(_) => bool;
  let isStale: t(_) => bool;
  let dataUpdatedAt: t(_) => float;
  let refetch: t(_) => Js.Promise.t(unit);
};

/** Never fetches on the server: [Success] when the query already settled in
    this request, [Pending] otherwise. */
let useQuery:
  (
    ~enabled: bool=?,
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    Query.t('key, 'data),
    'key
  ) =>
  QueryResult.t('data);

/** [None] skips the query, which stays [Pending] without fetching. */
let useQueryOpt:
  (
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    Query.t('key, 'data),
    option('key)
  ) =>
  QueryResult.t('data);

/** [select] runs on the cached data and should be stable across renders. */
let useQuerySelect:
  (
    ~select: 'data => 'selected,
    ~enabled: bool=?,
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    Query.t('key, 'data),
    'key
  ) =>
  QueryResult.t('selected);

/** Suspends until the query has data and throws its error to the nearest
    [ErrorBoundary]. On the server, the data streams to the browser ahead of
    the Suspense boundary that shows it. */
let useSuspenseQuery:
  (
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    Query.t('key, 'data),
    'key
  ) =>
  SuspenseResult.t('data);

module InfiniteResult: {
  type t('page, 'param);
  let status: t('page, 'param) => status(infiniteData('page, 'param));
  let data: t('page, 'param) => option(infiniteData('page, 'param));
  let error: t(_, _) => option(exn);
  let fetchStatus: t(_, _) => fetchStatus;
  let isFetching: t(_, _) => bool;
  let isStale: t(_, _) => bool;
  let dataUpdatedAt: t(_, _) => float;
  let refetch: t(_, _) => Js.Promise.t(unit);
  /** Computed from [getNextPageParam], so the server knows it too. */
  let hasNextPage: t(_, _) => bool;
  let hasPreviousPage: t(_, _) => bool;
  let isFetchingNextPage: t(_, _) => bool;
  let isFetchingPreviousPage: t(_, _) => bool;
  /** A failed page goes to [error] and keeps the pages already fetched. */
  let fetchNextPage: t(_, _) => Js.Promise.t(unit);
  let fetchPreviousPage: t(_, _) => Js.Promise.t(unit);
};

module SuspenseInfiniteResult: {
  type t('page, 'param);
  let data: t('page, 'param) => infiniteData('page, 'param);
  /** The error of the last refetch or page fetch, while [data] keeps the
      pages already fetched. */
  let refetchError: t(_, _) => option(exn);
  let fetchStatus: t(_, _) => fetchStatus;
  let isFetching: t(_, _) => bool;
  let isStale: t(_, _) => bool;
  let dataUpdatedAt: t(_, _) => float;
  let refetch: t(_, _) => Js.Promise.t(unit);
  let hasNextPage: t(_, _) => bool;
  let hasPreviousPage: t(_, _) => bool;
  let isFetchingNextPage: t(_, _) => bool;
  let isFetchingPreviousPage: t(_, _) => bool;
  let fetchNextPage: t(_, _) => Js.Promise.t(unit);
  let fetchPreviousPage: t(_, _) => Js.Promise.t(unit);
};

/** [useQuery] for an infinite query: never fetches on the server. */
let useInfiniteQuery:
  (
    ~enabled: bool=?,
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    InfiniteQuery.t('key, 'page, 'param),
    'key
  ) =>
  InfiniteResult.t('page, 'param);

/** [useSuspenseQuery] for an infinite query. On the server, it fetches the
    first page unless a [prefetchInfinite] already fetched more. */
let useSuspenseInfiniteQuery:
  (
    ~staleTime: StaleTime.t=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    InfiniteQuery.t('key, 'page, 'param),
    'key
  ) =>
  SuspenseInfiniteResult.t('page, 'param);

/** Several queries in one hook, each with its own key and data types. */
module Queries: {
  type t('results);
  let one: (Query.t('key, 'data), 'key) => t(QueryResult.t('data));
  let both: (t('a), t('b)) => t(('a, 'b));
  let all: array(t('a)) => t(array('a));
};

/** [useQuery] for each of [queries]. */
let useQueries: Queries.t('results) => 'results;

module SuspenseQueries: {
  type t('results);
  let one: (Query.t('key, 'data), 'key) => t(SuspenseResult.t('data));
  let both: (t('a), t('b)) => t(('a, 'b));
  let all: array(t('a)) => t(array('a));
};

/** Suspends until every one of [queries] has data. They all start at once,
    also on the server, instead of one after the other. */
let useSuspenseQueries: SuspenseQueries.t('results) => 'results;

/** Starts [query] during render unless the cache already has it, so a
    Suspense boundary below does not wait for its parent to suspend first.
    On the server, it streams to the browser like a [prefetch]. */
let usePrefetchQuery: (Query.t('key, 'data), 'key) => unit;

/** [usePrefetchQuery] for an infinite query, fetching [pages] pages. */
let usePrefetchInfiniteQuery:
  (~pages: int=?, InfiniteQuery.t('key, 'page, 'param), 'key) => unit;

module Mutation: {
  type t('variables, 'data);

  /** [name] is the mutation's key and must be unique. [onMutate]'s result
      reaches the other callbacks as [Some(context)], for instance to roll
      back an optimistic update. A callback's promise keeps the mutation
      pending until it settles. Mutations with the same [scope] run one at a
      time, in order. */
  let make:
    (
      ~name: string,
      ~fn: 'variables => Js.Promise.t('data),
      ~onMutate: (QueryClient.t, 'variables) => Js.Promise.t('context)=?,
      ~onSuccess:
        (QueryClient.t, 'data, 'variables, option('context)) =>
        Js.Promise.t(unit)
          =?,
      ~onError:
        (QueryClient.t, exn, 'variables, option('context)) =>
        Js.Promise.t(unit)
          =?,
      ~onSettled:
        (QueryClient.t, result('data, exn), 'variables, option('context)) =>
        Js.Promise.t(unit)
          =?,
      ~retry: Retry.t=?,
      ~retryDelay: (int, exn) => float=?,
      ~scope: string=?,
      unit
    ) =>
    t('variables, 'data);
};

type mutationStatus('variables, 'data) =
  | Idle
  | Running('variables)
  | Failed(exn, 'variables)
  | Succeeded('data, 'variables);

module MutationResult: {
  type t('variables, 'data);
  let status: t('variables, 'data) => mutationStatus('variables, 'data);
  let isPending: t(_, _) => bool;
  /** Errors go to [status] and the callbacks, not to the caller. These
      callbacks run after the mutation's own, and only while the component
      is mounted. */
  let mutate:
    (
      ~onSuccess: ('data, 'variables) => unit=?,
      ~onError: (exn, 'variables) => unit=?,
      ~onSettled: (result('data, exn), 'variables) => unit=?,
      t('variables, 'data),
      'variables
    ) =>
    unit;
  /** Rejects with the mutation's error. */
  let mutateAsync: (t('variables, 'data), 'variables) => Js.Promise.t('data);
  /** Back to [Idle]. */
  let reset: t(_, _) => unit;
};

/** Does nothing on the server. */
let useMutation:
  Mutation.t('variables, 'data) => MutationResult.t('variables, 'data);

/** Every run of this mutation in the client's cache, from any component,
    oldest first. Empty on the server. */
let useMutationState:
  Mutation.t('variables, 'data) => array(mutationStatus('variables, 'data));

/** How many mutations are running, only of [mutation] when given. Zero on the
    server. */
let useIsMutating: (~mutation: Mutation.t(_, _)=?, unit) => int;

/** A client's successful queries as JSON, to hydrate another client: for
    instance the browser's, from data the server put in the page. */
module DehydratedState: {
  type t;
  let toJson: t => Melange_json.t;
  /** Raises [Melange_json.Of_json_error] when [json] is not a dehydrated
      state. */
  let fromJson: Melange_json.t => t;
};

/** The client's successful queries. */
let dehydrate: QueryClient.t => DehydratedState.t;

/** Puts the state's queries in the client, except where it has newer data.
    A query whose data does not decode is left out. On the server, the
    hydrated queries also stream to the browser. */
let hydrate: (QueryClient.t, DehydratedState.t) => unit;

/** Hydrates [state] before its children render. In the browser, data for a
    query already in the cache waits until the render commits, so a
    transition does not change what is on screen. */
module HydrationBoundary: {
  [@react.component]
  let make:
    (~state: DehydratedState.t, ~children: React.element) => React.element;
};

/** Saves a client's successful queries somewhere and restores them in a
    later visit, like TanStack's persist-client packages. What is saved is
    [dehydrate]'s state, so each query's data goes through its codec. */
module Persister: {
  type t;

  type persistedClient = {
    timestamp: float,
    buster: string,
    clientState: DehydratedState.t,
  };

  /** Where [syncStorage] keeps the saved client. */
  module Storage: {
    type t;

    /** The page's [localStorage] and [sessionStorage]. On the server there
        are none, and a persister over them does nothing, like TanStack's in
        Node. */
    let local: t;
    let session: t;

    let make:
      (
        ~getItem: string => option(string),
        ~setItem: (string, string) => unit,
        ~removeItem: string => unit
      ) =>
      t;
  };

  /** TanStack's [createSyncStoragePersister]: the client as JSON under
      [key] (default ["REACT_QUERY_OFFLINE_CACHE"]), written at most once
      every [throttleTime] milliseconds (default 1000). [storage] defaults to
      [Storage.local]. */
  let syncStorage:
    (~storage: Storage.t=?, ~key: string=?, ~throttleTime: float=?, unit) => t;

  /** A persister of your own, such as one over IndexedDB. */
  let make:
    (
      ~persist: persistedClient => Js.Promise.t(unit),
      ~restore: unit => Js.Promise.t(option(persistedClient)),
      ~remove: unit => Js.Promise.t(unit)
    ) =>
    t;
};

/** Puts the saved client in [client] unless it is older than [maxAge]
    milliseconds (default a day) or was saved with another [buster] (default
    [""]), in which case it removes it. Like [hydrate], cached data that is
    newer stays. Rejects, after removing the saved client, when it cannot be
    read. */
let persistQueryClientRestore:
  (
    ~client: QueryClient.t,
    ~persister: Persister.t,
    ~maxAge: float=?,
    ~buster: string=?,
    unit
  ) =>
  Js.Promise.t(unit);

let persistQueryClientSave:
  (~client: QueryClient.t, ~persister: Persister.t, ~buster: string=?, unit) =>
  Js.Promise.t(unit);

/** Saves [client] whenever a query is added, removed or updated, until the
    returned function is called. The server's cache does not tell, so there
    it saves nothing. */
let persistQueryClientSubscribe:
  (
    ~client: QueryClient.t,
    ~persister: Persister.t,
    ~buster: string=?,
    unit,
    unit
  ) =>
  unit;

/** [persistQueryClientRestore], then [persistQueryClientSubscribe] unless
    the returned function was called first. The promise is the restore. */
let persistQueryClient:
  (
    ~client: QueryClient.t,
    ~persister: Persister.t,
    ~maxAge: float=?,
    ~buster: string=?,
    unit
  ) =>
  (unit => unit, Js.Promise.t(unit));

/** Whether a [PersistQueryClientProvider] above is still restoring. Then
    [useQuery] shows what the cache has without fetching, while
    [useSuspenseQuery] still fetches. True on the server and while
    hydrating, so the browser hydrates the server's markup. */
let useIsRestoring: unit => bool;

module IsRestoringProvider: {
  [@react.component]
  let make: (~value: bool, ~children: React.element) => React.element;
};

/** [QueryClient.Provider] that restores [client] from [persister] after
    hydration, then saves it on every change. [onSuccess] or [onError] runs
    when the restore settles. On the server it only provides [client], still
    restoring. */
module PersistQueryClientProvider: {
  [@react.component]
  let make:
    (
      ~client: QueryClient.t,
      ~persister: Persister.t,
      ~maxAge: float=?,
      ~buster: string=?,
      ~onSuccess: unit => unit=?,
      ~onError: unit => unit=?,
      ~children: React.element
    ) =>
    React.element;
};

/** Gives the queries under it their own reset: after [reset], a query that
    failed there fetches again when it mounts instead of throwing its error
    again. [ErrorBoundary]'s [retry] resets the nearest one. */
module QueryErrorResetBoundary: {
  type t;
  let reset: t => unit;
  let isReset: t => bool;
  let clearReset: t => unit;

  [@react.component]
  let make: (~children: React.element) => React.element;
};

let useQueryErrorResetBoundary: unit => QueryErrorResetBoundary.t;

/** Catches errors thrown by the queries under it. [retry] refetches them and
    renders the children again. On the server, failed queries render their
    Suspense fallback and run again in the browser. */
module ErrorBoundary: {
  [@react.component]
  let make:
    (
      ~fallback: (~error: exn, ~retry: unit => unit) => React.element,
      ~children: React.element
    ) =>
    React.element;
};

/** Whether the page has focus, for [refetchOnWindowFocus]. The browser
    follows the document's visibility unless [setFocused] overrides it. Like
    TanStack in Node, the server has one per process, and its page is
    focused. */
module FocusManager: {
  let isFocused: unit => bool;

  /** [None] goes back to following the document's visibility. Listeners
      hear about a change only. */
  let setFocused: option(bool) => unit;

  /** Calls the listener with [isFocused()] on each change, until the
      returned function unsubscribes it. */
  let subscribe: (bool => unit, unit) => unit;

  /** Replaces how focus changes are detected, at once. [setup] gets a
      function that sets the focus, or with [None] tells listeners to look
      again, and returns its cleanup. A [setup] that returns [None] runs
      again for the next subscriber. */
  let setEventListener:
    ((option(bool) => unit) => option(unit => unit)) => unit;
};

/** Whether the browser is online. Queries and mutations with
    [NetworkMode.Online] pause while it is offline, and
    [refetchOnReconnect] refetches when it comes back. Like TanStack in Node,
    the server has one per process, and it starts online; its queries never
    pause. */
module OnlineManager: {
  let isOnline: unit => bool;

  /** Listeners hear about a change only. */
  let setOnline: bool => unit;

  let subscribe: (bool => unit, unit) => unit;

  /** [FocusManager.setEventListener] for the online state. */
  let setEventListener: ((bool => unit) => option(unit => unit)) => unit;
};

/** Schedules the notifications of the cache and its observers, so React
    renders once for several changes. The default scheduler runs each
    notification on a later tick: [setTimeout(0)] in the browser, the next
    Lwt iteration on the server, which, like TanStack in Node, has one per
    process. */
module NotifyManager: {
  /** Runs [callback], holding the notifications it schedules until it
      returns, then schedules them together. Nested batches wait for the
      outermost one. */
  let batch: (unit => 'a) => 'a;

  /** [callback] with its calls scheduled. */
  let batchCalls: ('a => unit, 'a) => unit;
  let schedule: (unit => unit) => unit;

  /** Wraps each notification, for example in React's [act] in tests. */
  let setNotifyFunction: ((unit => unit) => unit) => unit;

  /** Wraps each batch of notifications, for example in
      [ReactDOM.unstable_batchedUpdates]. */
  let setBatchNotifyFunction: ((unit => unit) => unit) => unit;
  let setScheduler: ((unit => unit) => unit) => unit;
  let defaultScheduler: (unit => unit) => unit;
};

include ReactQuery_platform.S with type queryClient := QueryClient.t;
