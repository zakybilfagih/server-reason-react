/* 1:1 externals for @tanstack/react-query. */

type thrown;
type queryClient;
type queryFunctionContext;
type queryState;
type queryOptions('data, 'selected);
type queryResult('data);
type mutationFunctionContext;
type mutationOptions('variables, 'data, 'context);
type mutationResult('variables, 'data);
type errorResetBoundary;
type defaultQueryOptions;
type skipToken;

type query;
type mutation;
type queryCache;
type mutationCache;
type clientConfig;

type defaultOptions = {queries: defaultQueryOptions};

[@mel.obj]
external clientConfig:
  (
    ~defaultOptions: defaultOptions,
    ~queryCache: queryCache=?,
    ~mutationCache: mutationCache=?,
    unit
  ) =>
  clientConfig;

[@mel.module "@tanstack/react-query"] [@mel.new]
external makeQueryClient: clientConfig => queryClient = "QueryClient";

type queryCacheConfig = {onError: (. thrown, query) => unit};

[@mel.module "@tanstack/react-query"] [@mel.new]
external makeQueryCache: queryCacheConfig => queryCache = "QueryCache";

type mutationCacheConfig = {
  onError: (. thrown, Js.Json.t, Js.Json.t, mutation) => unit,
};

[@mel.module "@tanstack/react-query"] [@mel.new]
external makeMutationCache: mutationCacheConfig => mutationCache =
  "MutationCache";

[@mel.get] external queryHash: query => string = "queryHash";
[@mel.get] external queryKeyOf: query => Js.Json.t = "queryKey";
[@mel.get] external stateOf: query => queryState = "state";
[@mel.get] external queryStateStatus: queryState => string = "status";

type mutationOptionsOf;
[@mel.get]
external mutationOptionsOf: mutation => mutationOptionsOf = "options";
[@mel.get]
external mutationKeyOf: mutationOptionsOf => Js.undefined(Js.Json.t) =
  "mutationKey";

[@mel.send]
external getQueryCache: queryClient => queryCache = "getQueryCache";

[@mel.send] external getAllQueries: queryCache => array(query) = "getAll";

[@mel.module "@tanstack/react-query"]
external dehydrateQuery: (query, (. 'data) => Js.Json.t) => Js.Json.t =
  "dehydrateQuery";

/* useQueries takes queries of different data types in one array. */
type anyQueryOptions;
external anyQueryOptions: queryOptions(_, _) => anyQueryOptions = "%identity";

type queriesOptions = {queries: array(anyQueryOptions)};
type queriesResult;

[@mel.module "@tanstack/react-query"]
external useQueries: queriesOptions => queriesResult = "useQueries";

[@mel.module "@tanstack/react-query"]
external useSuspenseQueries: queriesOptions => queriesResult =
  "useSuspenseQueries";

[@mel.get_index]
external resultAt: (queriesResult, int) => queryResult('data);

[@mel.send]
external subscribeQueryCache:
  (queryCache, (. unit) => unit) => (. unit) => unit =
  "subscribe";

type focusManager;

[@mel.module "@tanstack/react-query"]
external focusManager: focusManager = "focusManager";

[@mel.send] external isFocused: focusManager => bool = "isFocused";

[@mel.send]
external setFocused: (focusManager, Js.undefined(bool)) => unit =
  "setFocused";

[@mel.send]
external subscribeFocus: (focusManager, (. bool) => unit) => (. unit) => unit =
  "subscribe";

[@mel.send]
external setFocusEventListener:
  (
    focusManager,
    (. ((. Js.undefined(bool)) => unit)) => Js.undefined((. unit) => unit)
  ) =>
  unit =
  "setEventListener";

type onlineManager;

[@mel.module "@tanstack/react-query"]
external onlineManager: onlineManager = "onlineManager";

[@mel.send] external isOnline: onlineManager => bool = "isOnline";

[@mel.send] external setOnline: (onlineManager, bool) => unit = "setOnline";

[@mel.send]
external subscribeOnline: (onlineManager, (. bool) => unit) => (. unit) => unit =
  "subscribe";

[@mel.send]
external setOnlineEventListener:
  (onlineManager, (. ((. bool) => unit)) => Js.undefined((. unit) => unit)) =>
  unit =
  "setEventListener";

type notifyManager;

[@mel.module "@tanstack/react-query"]
external notifyManager: notifyManager = "notifyManager";

[@mel.send]
external batchCalls: (notifyManager, (. unit) => unit) => (. unit) => unit =
  "batchCalls";

[@mel.send]
external batchCallsWith: (notifyManager, (. 'a) => unit) => (. 'a) => unit =
  "batchCalls";

[@mel.send] external batch: (notifyManager, (. unit) => 'a) => 'a = "batch";

[@mel.send]
external schedule: (notifyManager, (. unit) => unit) => unit = "schedule";

type notifyFunction = (. ((. unit) => unit)) => unit;

[@mel.send]
external setNotifyFunction: (notifyManager, notifyFunction) => unit =
  "setNotifyFunction";

[@mel.send]
external setBatchNotifyFunction: (notifyManager, notifyFunction) => unit =
  "setBatchNotifyFunction";

[@mel.send]
external setScheduler: (notifyManager, notifyFunction) => unit =
  "setScheduler";

[@mel.module "@tanstack/react-query"]
external defaultScheduler: notifyFunction = "defaultScheduler";

type asyncIterable('chunk);

/* A pull function as an async iterable, for [streamedQuery]. */
let asyncIterable:
  (unit => Js.Promise.t(Js.undefined('chunk))) => asyncIterable('chunk) = [%mel.raw
  {|
  function (next) {
    return {
      [Symbol.asyncIterator]() {
        return {
          next: () =>
            next().then((value) =>
              value === undefined ? { done: true, value } : { done: false, value },
            ),
        };
      },
    };
  }
|}
];

type streamedQueryParams('chunk, 'data);

[@mel.obj]
external streamedQueryParams:
  (
    ~streamFn: queryFunctionContext => Js.Promise.t(asyncIterable('chunk)),
    ~reducer: (. 'data, 'chunk) => 'data,
    ~initialValue: 'data,
    ~refetchMode: string,
    unit
  ) =>
  streamedQueryParams('chunk, 'data);

[@mel.module "@tanstack/react-query"]
external streamedQuery:
  streamedQueryParams('chunk, 'data) =>
  (. queryFunctionContext) => Js.Promise.t('data) =
  "experimental_streamedQuery";

[@mel.obj]
external defaultQueryOptions:
  (
    ~staleTime: Js.Json.t=?,
    ~gcTime: float=?,
    ~retry: (. int, thrown) => bool=?,
    ~retryDelay: (. int, thrown) => float=?,
    ~networkMode: string=?,
    ~refetchOnWindowFocus: bool=?,
    ~refetchOnReconnect: bool=?,
    unit
  ) =>
  defaultQueryOptions;

[@mel.obj]
external queryOptions:
  (
    ~queryKey: Js.Json.t,
    ~queryFn: queryFunctionContext => Js.Promise.t('data),
    ~staleTime: Js.Json.t=?,
    ~gcTime: float=?,
    ~retry: (. int, thrown) => bool=?,
    ~retryDelay: (. int, thrown) => float=?,
    ~networkMode: string=?,
    ~enabled: bool=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    ~select: 'data => 'selected=?,
    unit
  ) =>
  queryOptions('data, 'selected);

[@mel.module "@tanstack/react-query"]
external skipToken: skipToken = "skipToken";

[@mel.obj]
external skipQueryOptions:
  (~queryKey: Js.Json.t, ~queryFn: skipToken, unit) =>
  queryOptions('data, 'selected);

[@mel.get] external signal: queryFunctionContext => Fetch.signal = "signal";
[@mel.get] external pageParam: queryFunctionContext => 'param = "pageParam";

type infiniteQueryOptions('page, 'param);

[@mel.obj]
external infiniteQueryOptions:
  (
    ~queryKey: Js.Json.t,
    ~queryFn: queryFunctionContext => Js.Promise.t('page),
    ~initialPageParam: 'param,
    ~getNextPageParam:
      (. 'page, array('page), 'param, array('param)) => Js.undefined('param),
    ~getPreviousPageParam:
      (. 'page, array('page), 'param, array('param)) => Js.undefined('param)
        =?,
    ~maxPages: int=?,
    ~pages: int=?,
    ~staleTime: Js.Json.t=?,
    ~gcTime: float=?,
    ~retry: (. int, thrown) => bool=?,
    ~retryDelay: (. int, thrown) => float=?,
    ~networkMode: string=?,
    ~enabled: bool=?,
    ~refetchInterval: float=?,
    ~refetchOnWindowFocus: bool=?,
    unit
  ) =>
  infiniteQueryOptions('page, 'param);

[@mel.module "@tanstack/react-query"]
external useInfiniteQuery: infiniteQueryOptions(_, _) => queryResult('data) =
  "useInfiniteQuery";

[@mel.module "@tanstack/react-query"]
external useSuspenseInfiniteQuery:
  infiniteQueryOptions(_, _) => queryResult('data) =
  "useSuspenseInfiniteQuery";

[@mel.send]
external prefetchInfiniteQuery:
  (queryClient, infiniteQueryOptions(_, _)) => Js.Promise.t(unit) =
  "prefetchInfiniteQuery";

[@mel.send]
external fetchInfiniteQuery:
  (queryClient, infiniteQueryOptions(_, _)) => Js.Promise.t('data) =
  "fetchInfiniteQuery";

[@mel.get] external hasNextPage: queryResult(_) => bool = "hasNextPage";
[@mel.get]
external hasPreviousPage: queryResult(_) => bool = "hasPreviousPage";
[@mel.get]
external isFetchingNextPage: queryResult(_) => bool = "isFetchingNextPage";
[@mel.get]
external isFetchingPreviousPage: queryResult(_) => bool =
  "isFetchingPreviousPage";
[@mel.send]
external fetchNextPage: queryResult(_) => Js.Promise.t(queryResult(_)) =
  "fetchNextPage";
[@mel.send]
external fetchPreviousPage: queryResult(_) => Js.Promise.t(queryResult(_)) =
  "fetchPreviousPage";

[@mel.module "@tanstack/react-query"]
external hashKey: Js.Json.t => string = "hashKey";

[@mel.module "@tanstack/react-query"]
external useQuery: queryOptions('data, 'selected) => queryResult('selected) =
  "useQuery";

[@mel.module "@tanstack/react-query"]
external useSuspenseQuery:
  queryOptions('data, 'selected) => queryResult('selected) =
  "useSuspenseQuery";

[@mel.module "@tanstack/react-query"]
external useQueryClient: unit => queryClient = "useQueryClient";

[@mel.module "@tanstack/react-query"]
external useQueryErrorResetBoundary: unit => errorResetBoundary =
  "useQueryErrorResetBoundary";

[@mel.send] external resetErrorBoundary: errorResetBoundary => unit = "reset";
[@mel.send]
external isResetErrorBoundary: errorResetBoundary => bool = "isReset";
[@mel.send]
external clearResetErrorBoundary: errorResetBoundary => unit = "clearReset";

[@mel.module "@tanstack/react-query"]
external usePrefetchQuery: queryOptions('data, 'data) => unit =
  "usePrefetchQuery";

[@mel.module "@tanstack/react-query"]
external usePrefetchInfiniteQuery: infiniteQueryOptions(_, _) => unit =
  "usePrefetchInfiniteQuery";

[@mel.get] external status: queryResult(_) => string = "status";
[@mel.get] external fetchStatus: queryResult(_) => string = "fetchStatus";
[@mel.get] external data: queryResult('data) => Js.undefined('data) = "data";
[@mel.get] external error: queryResult(_) => Js.null(thrown) = "error";
[@mel.get] external definedData: queryResult('data) => 'data = "data";
[@mel.get] external isFetching: queryResult(_) => bool = "isFetching";
[@mel.get] external isStale: queryResult(_) => bool = "isStale";
[@mel.get] external dataUpdatedAt: queryResult(_) => float = "dataUpdatedAt";
[@mel.send]
external refetch: queryResult(_) => Js.Promise.t(queryResult(_)) =
  "refetch";

[@mel.send]
external prefetchQuery:
  (queryClient, queryOptions('data, 'data)) => Js.Promise.t(unit) =
  "prefetchQuery";

type queryFilters;

[@mel.obj]
external queryFilters:
  (
    ~queryKey: Js.Json.t=?,
    ~exact: bool=?,
    ~type_: [@mel.as "type"] string=?,
    ~stale: bool=?,
    ~fetchStatus: string=?,
    ~refetchType: string=?,
    unit
  ) =>
  queryFilters;

[@mel.send]
external fetchQuery:
  (queryClient, queryOptions('data, 'data)) => Js.Promise.t('data) =
  "fetchQuery";

[@mel.send]
external ensureQueryData:
  (queryClient, queryOptions('data, 'data)) => Js.Promise.t('data) =
  "ensureQueryData";

[@mel.send]
external invalidateQueries: (queryClient, queryFilters) => Js.Promise.t(unit) =
  "invalidateQueries";

[@mel.send]
external refetchQueries: (queryClient, queryFilters) => Js.Promise.t(unit) =
  "refetchQueries";

[@mel.send]
external cancelQueries: (queryClient, queryFilters) => Js.Promise.t(unit) =
  "cancelQueries";

[@mel.send]
external removeQueries: (queryClient, queryFilters) => unit = "removeQueries";

[@mel.send]
external resetQueries: (queryClient, queryFilters) => Js.Promise.t(unit) =
  "resetQueries";

[@mel.send] external clear: queryClient => unit = "clear";

[@mel.send]
external countFetching: (queryClient, queryFilters) => int = "isFetching";

[@mel.send]
external getQueriesData:
  (queryClient, queryFilters) => array((Js.Json.t, Js.undefined('data))) =
  "getQueriesData";

[@mel.send]
external setQueriesData:
  (queryClient, queryFilters, Js.undefined('data) => Js.undefined('data)) =>
  unit =
  "setQueriesData";

[@mel.send]
external getQueryData: (queryClient, Js.Json.t) => Js.undefined('data) =
  "getQueryData";

[@mel.send]
external updateQueryData:
  (queryClient, Js.Json.t, Js.undefined('data) => Js.undefined('data)) =>
  unit =
  "setQueryData";

[@mel.send]
external getQueryState: (queryClient, Js.Json.t) => Js.undefined(queryState) =
  "getQueryState";

[@mel.get] external stateDataUpdatedAt: queryState => float = "dataUpdatedAt";
[@mel.get]
external queryStateData: queryState => Js.undefined('data) = "data";
[@mel.get] external queryStateError: queryState => Js.null(thrown) = "error";
[@mel.get]
external queryStateErrorUpdatedAt: queryState => float = "errorUpdatedAt";
[@mel.get]
external queryStateFetchStatus: queryState => string = "fetchStatus";
[@mel.get]
external queryStateIsInvalidated: queryState => bool = "isInvalidated";

type mutationScope = {id: string};

[@mel.obj]
external mutationOptions:
  (
    ~mutationKey: Js.Json.t,
    ~mutationFn: 'variables => Js.Promise.t('data),
    ~onMutate:
      (. 'variables, mutationFunctionContext) => Js.Promise.t('context)=?,
    ~onSuccess:
      (
        . 'data,
        'variables,
        Js.undefined('context),
        mutationFunctionContext
      ) =>
      Js.Promise.t(unit)
        =?,
    ~onError:
      (
        . thrown,
        'variables,
        Js.undefined('context),
        mutationFunctionContext
      ) =>
      Js.Promise.t(unit)
        =?,
    ~onSettled:
      (
        . 'data,
        Js.null(thrown),
        'variables,
        Js.undefined('context),
        mutationFunctionContext
      ) =>
      Js.Promise.t(unit)
        =?,
    ~retry: (. int, thrown) => bool=?,
    ~retryDelay: (. int, thrown) => float=?,
    ~scope: mutationScope=?,
    unit
  ) =>
  mutationOptions('variables, 'data, 'context);

[@mel.get]
external mutationClient: mutationFunctionContext => queryClient = "client";

[@mel.module "@tanstack/react-query"]
external useMutation:
  mutationOptions('variables, 'data, _) => mutationResult('variables, 'data) =
  "useMutation";

[@mel.get] external mutationStatus: mutationResult(_, _) => string = "status";
[@mel.get]
external mutationVariables: mutationResult('variables, _) => 'variables =
  "variables";
[@mel.get] external mutationData: mutationResult(_, 'data) => 'data = "data";
[@mel.get]
external mutationError: mutationResult(_, _) => Js.null(thrown) = "error";
[@mel.get]
external mutationIsPending: mutationResult(_, _) => bool = "isPending";
type mutateOptions('variables, 'data);

[@mel.obj]
external mutateOptions:
  (
    ~onSuccess: (. 'data, 'variables) => unit=?,
    ~onError: (. thrown, 'variables) => unit=?,
    ~onSettled: (. 'data, Js.null(thrown), 'variables) => unit=?,
    unit
  ) =>
  mutateOptions('variables, 'data);

[@mel.send]
external mutate:
  (
    mutationResult('variables, 'data),
    'variables,
    mutateOptions('variables, 'data)
  ) =>
  unit =
  "mutate";
[@mel.send]
external mutateAsync:
  (mutationResult('variables, 'data), 'variables) => Js.Promise.t('data) =
  "mutateAsync";
[@mel.send] external resetMutation: mutationResult(_, _) => unit = "reset";

type mutationFilters = {
  mutationKey: Js.Json.t,
  exact: bool,
};

type mutationStateOptions = {filters: mutationFilters};
type mutationState('variables, 'data);

[@mel.module "@tanstack/react-query"]
external useMutationState:
  mutationStateOptions => array(mutationState('variables, 'data)) =
  "useMutationState";

[@mel.module "@tanstack/react-query"]
external useIsMutating: Js.undefined(mutationFilters) => int =
  "useIsMutating";

[@mel.get] external stateStatus: mutationState(_, _) => string = "status";
[@mel.get]
external stateVariables: mutationState('variables, _) => 'variables =
  "variables";
[@mel.get] external stateData: mutationState(_, 'data) => 'data = "data";
[@mel.get]
external stateError: mutationState(_, _) => Js.null(thrown) = "error";

type dehydratedQuery;
type dehydratedState = {queries: array(dehydratedQuery)};

[@mel.module "@tanstack/react-query"]
external hydrate: (queryClient, dehydratedState) => unit = "hydrate";

/* A query from the server with its decoded data or the promise of it. */
let dehydratedQuery:
  (Js.Json.t, Js.undefined('data), Js.undefined(Js.Promise.t('data))) =>
  dehydratedQuery = [%mel.raw
  {|
  function (message, data, promise) {
    return { ...message, state: { ...message.state, data }, promise };
  }
|}
];

type deferred('a) = {
  promise: Js.Promise.t('a),
  resolve: (. 'a) => unit,
  redact: (. unit) => unit,
};

/* Rejects with TanStack's redacted error, as [dehydrate] does by default. */
let deferred: unit => deferred('a) = [%mel.raw
  {|
  function () {
    let resolve, redact;
    const promise = new Promise((res, rej) => {
      resolve = res;
      redact = () => rej(new Error("redacted"));
    });
    return { promise, resolve, redact };
  }
|}
];

type persisted('state) = {
  timestamp: float,
  buster: string,
  clientState: 'state,
};

/* What a persister method returns: a value or a promise of one. */
type promisable('a);

[@mel.scope "Promise"]
external promisable: promisable('a) => Js.Promise.t('a) = "resolve";

type persister;

[@mel.obj]
external persister:
  (
    ~persistClient: (. persisted(Js.Json.t)) => Js.Promise.t(unit),
    ~restoreClient:
      (. unit) => Js.Promise.t(Js.undefined(persisted('state))),
    ~removeClient: (. unit) => Js.Promise.t(unit)
  ) =>
  persister;

[@mel.send]
external persistClient: (persister, persisted(Js.Json.t)) => promisable(unit) =
  "persistClient";

[@mel.send]
external restoreClient:
  persister => promisable(Js.undefined(persisted(Js.Json.t))) =
  "restoreClient";

[@mel.send]
external removeClient: persister => promisable(unit) = "removeClient";

type storage;

[@mel.obj]
external storage:
  (
    ~getItem: (. string) => Js.null(string),
    ~setItem: (. string, string) => unit,
    ~removeItem: (. string) => unit
  ) =>
  storage;

/* Read when a persister is made, so that importing this module needs no
   window. A storage the browser refuses to open counts as none. */
let windowStorage: string => Js.undefined(storage) = [%mel.raw
  {|
  function (name) {
    try {
      return typeof window === "undefined" ? undefined : window[name];
    } catch (error) {
      return undefined;
    }
  }
|}
];

type syncStorageOptions;

[@mel.obj]
external syncStorageOptions:
  (
    ~storage: Js.undefined(storage),
    ~key: string=?,
    ~throttleTime: float=?,
    unit
  ) =>
  syncStorageOptions;

[@mel.module "@tanstack/query-sync-storage-persister"]
external createSyncStoragePersister: syncStorageOptions => persister =
  "createSyncStoragePersister";

type persistOptions;

[@mel.obj]
external persistOptions:
  (
    ~queryClient: queryClient=?,
    ~persister: persister,
    ~maxAge: float=?,
    ~buster: string=?,
    unit
  ) =>
  persistOptions;

[@mel.module "@tanstack/react-query-persist-client"]
external persistQueryClientRestore: persistOptions => Js.Promise.t(unit) =
  "persistQueryClientRestore";

[@mel.module "@tanstack/react-query-persist-client"]
external persistQueryClientSave: persistOptions => Js.Promise.t(unit) =
  "persistQueryClientSave";

[@mel.module "@tanstack/react-query-persist-client"]
external persistQueryClientSubscribe: persistOptions => (. unit) => unit =
  "persistQueryClientSubscribe";

[@mel.module "@tanstack/react-query-persist-client"]
external persistQueryClient:
  persistOptions => ((. unit) => unit, Js.Promise.t(unit)) =
  "persistQueryClient";

module PersistQueryClientProvider = {
  [@mel.module "@tanstack/react-query-persist-client"] [@react.component]
  external make:
    (
      ~client: queryClient,
      ~persistOptions: persistOptions,
      ~onSuccess: unit => unit=?,
      ~onError: unit => unit=?,
      ~children: React.element
    ) =>
    React.element =
    "PersistQueryClientProvider";
};

[@mel.module "@tanstack/react-query"]
external useIsRestoring: unit => bool = "useIsRestoring";

module IsRestoringProvider = {
  [@mel.module "@tanstack/react-query"] [@react.component]
  external make: (~value: bool, ~children: React.element) => React.element =
    "IsRestoringProvider";
};

module QueryErrorResetBoundary = {
  [@mel.module "@tanstack/react-query"] [@react.component]
  external make: (~children: React.element) => React.element =
    "QueryErrorResetBoundary";
};

module HydrationBoundary = {
  [@mel.module "@tanstack/react-query"] [@react.component]
  external make:
    (~state: dehydratedState, ~children: React.element) => React.element =
    "HydrationBoundary";
};

module QueryClientProvider = {
  [@mel.module "@tanstack/react-query"] [@react.component]
  external make:
    (~client: queryClient, ~children: React.element) => React.element =
    "QueryClientProvider";
};

type componentClass;

[@mel.module "react"] external component: componentClass = "Component";

type errorBoundaryProps = {
  children: React.element,
  fallback: (. thrown, unit => unit) => React.element,
};

/* reason-react's error boundary cannot be reset, so retrying needs its own. */
let makeErrorBoundary: componentClass => React.component(errorBoundaryProps) = [%mel.raw
  {|
  function (Component) {
    class ErrorBoundary extends Component {
      constructor(props) {
        super(props);
        this.state = { failed: false, error: undefined };
        this.reset = () => this.setState({ failed: false, error: undefined });
      }
      static getDerivedStateFromError(error) {
        return { failed: true, error };
      }
      render() {
        return this.state.failed
          ? this.props.fallback(this.state.error, this.reset)
          : this.props.children;
      }
    }
    return ErrorBoundary;
  }
|}
];

let errorBoundary = makeErrorBoundary(component);
