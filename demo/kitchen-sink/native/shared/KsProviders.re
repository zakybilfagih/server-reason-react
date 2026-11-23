/* The example's App: the sandbox panel, then the QueryClientProvider around
   the router. Settings start from the cookies the server read and are
   written back like useSessionStorage writes sessionStorage. */

/* Every navigation tells the loaders which queries the browser has data
   for, so their ensureQueryData returns the cached data without fetching,
   as it does in the browser. */
let queryCache = client => [
  (
    KsData.queryCacheHeader,
    KsData.queryCacheToHeader(
      ReactQuery.QueryClient.cachedQueries(client)
      |> Array.to_list
      |> List.filter(ReactQuery.CachedQuery.hasData)
      |> List.map(ReactQuery.CachedQuery.hash),
    ),
  ),
];

let useSetting = (name, initial) => {
  let (value, setValue) = React.useState(() => initial);
  let%melange_only save = () => {
    let write: (string, string) => unit = [%mel.raw
      {js|(name, value) => { document.cookie = name + "=" + value + "; path=/; SameSite=Lax"; }|js}
    ];
    write(name, Int.to_string(value));
    None;
  };
  React.useEffect1(save, [|value|]);
  (value, next => setValue(_ => next));
};

[@react.client.component]
let make =
    (
      ~loaderDelay: option(int),
      ~pendingMs: option(int),
      ~pendingMinMs: option(int),
      ~children: React.element,
    ) => {
  let (client, _) = React.useState(KsData.makeQueryClient);
  let (loaderDelay, setLoaderDelay) =
    useSetting("loaderDelay", Option.value(~default=500, loaderDelay));
  let (pendingMs, setPendingMs) =
    useSetting("pendingMs", Option.value(~default=1000, pendingMs));
  let (pendingMinMs, setPendingMinMs) =
    useSetting("pendingMinMs", Option.value(~default=500, pendingMinMs));
  let (isPending, startTransition) = React.useTransition();
  let (target, setTarget) = React.useState(() => None);
  let value: KsSettings.t = {
    loaderDelay,
    setLoaderDelay,
    pendingMs,
    setPendingMs,
    pendingMinMs,
    setPendingMinMs,
    transitionTarget: isPending ? target : None,
    startTransition: (location, update) => {
      setTarget(_ => Some(location));
      startTransition(update);
    },
  };
  <KsSettings.Provider value>
    <KsSandbox />
    <ReactQuery.QueryClient.Provider client>
      <RouterReact.RequestHeaders value={() => queryCache(client)}>
        children
      </RouterReact.RequestHeaders>
    </ReactQuery.QueryClient.Provider>
  </KsSettings.Provider>;
};
