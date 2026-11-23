open ReactQuery;

let title = () =>
  switch (QueryResult.status(useQuery(Fixtures.note, 1))) {
  | Success(title) => title
  | Pending => "loading"
  | Error(_) => "failed"
  };

let length = () =>
  switch (QueryResult.status(useQuerySelect(~select=String.length, Fixtures.note, 1))) {
  | Success(length) => length
  | Pending
  | Error(_) => 0
  };

let skipped = (id: option(int)) => QueryResult.data(useQueryOpt(Fixtures.note, id));

let optimistic =
  Mutation.make(
    ~name="optimistic",
    ~fn=(title: string) => Lwt.return(String.length(title)),
    ~onMutate=(_, _) => Lwt.return(42),
    ~onError=(_, _, _, (previous: option(int))) => Lwt.return(ignore(previous)),
    (),
  );

let renameNow = (title: string) => {
  let rename = useMutation(Fixtures.rename);
  MutationResult.mutate(
    ~onSuccess=(length: int, (_: int, _: string)) => ignore(length),
    ~onSettled=(result: result(int, exn), _) => ignore(result),
    rename,
    (1, title),
  );
};

let saving = () =>
  useMutationState(Fixtures.rename)
  |> Array.to_list
  |> List.filter_map(
       fun
       | Running((id, title)) => Some(Int.to_string(id) ++ title)
       | Idle
       | Failed(_)
       | Succeeded(_) => None,
     );

let shout = client =>
  QueryClient.setQueryData(client, Fixtures.note, 1, title =>
    Option.map(String.uppercase_ascii, title)
  );

let firstPage = () =>
  switch (InfiniteResult.status(useInfiniteQuery(Fixtures.feed, "me"))) {
  | Success({ pages, pageParams }) => pages[0] ++ Int.to_string(pageParams[0])
  | Pending
  | Error(_) => ""
  };

let pair = () => {
  let (title, page) =
    useQueries(
      Queries.both(Queries.one(Fixtures.note, 1), Queries.one(Fixtures.note, 2)),
    );
  Option.value(QueryResult.data(title), ~default="") ++ Option.value(QueryResult.data(page), ~default="");
};

let titles = (ids: array(int)) =>
  useSuspenseQueries(SuspenseQueries.all(Array.map(id => SuspenseQueries.one(Fixtures.note, id), ids)))
  |> Array.map(SuspenseResult.data);

let copy = (from, into) =>
  hydrate(into, DehydratedState.fromJson(DehydratedState.toJson(dehydrate(from))));

let prefetchBoth = () => {
  usePrefetchQuery(Fixtures.note, 1);
  usePrefetchInfiniteQuery(~pages=2, Fixtures.feed, "me");
};

let resetAll = () => {
  let boundary = useQueryErrorResetBoundary();
  if (!QueryErrorResetBoundary.isReset(boundary)) {
    QueryErrorResetBoundary.reset(boundary);
  };
};

let letters =
  StreamedQuery.make(
    ~name="letters",
    ~key=(word: string) => [Key.string(word)],
    ~codec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~stream=
      (~signal as _, word) =>
        Lwt.return(
          Stream.fromArray(Array.init(String.length(word), String.get(word))),
        ),
    ~reducer=(text, letter) => text ++ String.make(1, letter),
    ~initialValue="",
    ~refetchMode=StreamedQuery.Append,
    (),
  );

let spelled = () =>
  QueryResult.data(useQuery(letters, "hello")) |> Option.value(~default="");

let quietly = (client, title) =>
  NotifyManager.batch(() => {
    QueryClient.setQueryData(client, Fixtures.note, 1, _ => Some(title));
    NotifyManager.batchCalls(ignore, title);
    String.length(title);
  });
