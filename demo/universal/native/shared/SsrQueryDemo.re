open Melange_json.Primitives;

[@deriving json]
type note =
  Note.t = {
    id: int,
    title: string,
    content: string,
    updated_at: float,
  };

[@deriving json]
type loaded = {
  note,
  loadedBy: string,
};

[@deriving json]
type rename = {title: string};

[@deriving json]
type notePage = {
  notes: array(note),
  nextCursor: option(int),
};

let maxTitleLength = 40;

let api = id => Routes.ssrQueryApi ++ "/" ++ Int.to_string(id);

/* A failed response rejects with its body as the message. */
let decodeResponse = ({ IsoFetch.ok, body, _ }) =>
  if (ok) {
    switch (loaded_of_json(Melange_json.of_string(body))) {
    | data => Js.Promise.resolve(data)
    | exception _ =>
      Js.Promise.reject(Failure("Unexpected response: " ++ body))
    };
  } else {
    Js.Promise.reject(Failure(body));
  };

let notFound = id => "Note " ++ Int.to_string(id) ++ " does not exist";

/* The same query function runs on the server and in the browser. */
let noteQuery =
  ReactQuery.Query.make(
    ~name="note",
    ~key=id => [ReactQuery.Key.int(id)],
    ~codec=
      ReactQuery.Codec.make(~encode=loaded_to_json, ~decode=loaded_of_json),
    ~fetch=
      (~signal, id) =>
        IsoFetch.request(~signal, api(id))
        |> Js.Promise.then_(decodeResponse),
    (),
  );

let renameNote = (id, title) =>
  IsoFetch.request(
    ~method_=`POST,
    ~body=Melange_json.to_string(rename_to_json({ title: title })),
    api(id),
  )
  |> Js.Promise.then_(decodeResponse);

/* Shows the new title at once, puts the old one back when the server rejects
   it, and refetches the note either way. */
let renameMutation =
  ReactQuery.Mutation.make(
    ~name="rename-note",
    ~fn=((id, title)) => renameNote(id, title),
    ~onMutate=
      (client, (id, title)) =>
        ReactQuery.QueryClient.cancel(
          client,
          ReactQuery.Filter.exact(noteQuery, id),
        )
        |> Js.Promise.then_(() => {
             let previous =
               ReactQuery.QueryClient.getQueryData(client, noteQuery, id);
             ReactQuery.QueryClient.setQueryData(
               client,
               noteQuery,
               id,
               Option.map(loaded =>
                 {
                   note: {
                     ...loaded.note,
                     title,
                   },
                   loadedBy: "an optimistic update",
                 }
               ),
             );
             Js.Promise.resolve(previous);
           }),
    ~onError=
      (client, _, (id, _), previous) => {
        switch (Option.join(previous)) {
        | Some(previous) =>
          ReactQuery.QueryClient.setQueryData(client, noteQuery, id, _ =>
            Some(previous)
          )
        | None => ()
        };
        Js.Promise.resolve();
      },
    ~onSettled=
      (client, _, (id, _), _) =>
        ReactQuery.QueryClient.invalidate(
          client,
          ReactQuery.Filter.exact(noteQuery, id),
        ),
    (),
  );

let pagesApi = "/demo/ssr-query/api/pages";

let decodePage = ({ IsoFetch.ok, body, _ }) =>
  if (ok) {
    switch (notePage_of_json(Melange_json.of_string(body))) {
    | page => Js.Promise.resolve(page)
    | exception _ =>
      Js.Promise.reject(Failure("Unexpected response: " ++ body))
    };
  } else {
    Js.Promise.reject(Failure(body));
  };

/* One note per page, so the list grows one note per click. */
let notesQuery =
  ReactQuery.InfiniteQuery.make(
    ~name="note-pages",
    ~key=() => [],
    ~pageCodec=
      ReactQuery.Codec.make(
        ~encode=notePage_to_json,
        ~decode=notePage_of_json,
      ),
    ~paramCodec=
      ReactQuery.Codec.make(~encode=int_to_json, ~decode=int_of_json),
    ~initialPageParam=0,
    ~fetchPage=
      (~signal, (), cursor) =>
        IsoFetch.request(~signal, pagesApi ++ "/" ++ Int.to_string(cursor))
        |> Js.Promise.then_(decodePage),
    ~getNextPageParam=
      ({ pages, _ }) =>
        Option.bind(
          Array.length(pages) == 0
            ? None : Some(pages[Array.length(pages) - 1]),
          page =>
          page.nextCursor
        ),
    (),
  );

/* The browser keeps the cache in localStorage; the server has none. */
let persister = ReactQuery.Persister.syncStorage(~key="ssr-query-demo", ());

let noteIds = [|1, 2, 3, 4|];
let peekId = 0;
let titleIds = [|1, 2, 3|];

/* Runs in the request handler before rendering, so every query starts at
   once. */
let prefetch = client => {
  Array.iter(
    id => ignore(ReactQuery.QueryClient.prefetch(client, noteQuery, id)),
    noteIds,
  );
};

let wait = ms =>
  switch%platform () {
  | Native => Lwt_unix.sleep(float_of_int(ms) /. 1000.)
  | Melange =>
    let done_ = ();
    Js.Promise.make((~resolve, ~reject as _) =>
      ignore(Js.Global.setTimeout(~f=() => resolve(. done_), ms))
    );
  };

let sentence = "Each word of this sentence is one chunk of a streamed query";

/* One word every 150ms, on the server and in the browser. */
let sentenceQuery =
  ReactQuery.StreamedQuery.make(
    ~name="sentence",
    ~key=() => [],
    ~codec=
      ReactQuery.Codec.make(~encode=string_to_json, ~decode=string_of_json),
    ~stream=
      (~signal as _, ()) => {
        let words = ref(String.split_on_char(' ', sentence));
        Js.Promise.resolve(
          ReactQuery.Stream.make(() =>
            wait(150)
            |> Js.Promise.then_(() =>
                 switch (words^) {
                 | [] => Js.Promise.resolve(None)
                 | [word, ...rest] =>
                   words := rest;
                   Js.Promise.resolve(Some(word));
                 }
               )
          ),
        );
      },
    ~reducer=(text, word) => text == "" ? word : text ++ " " ++ word,
    ~initialValue="",
    (),
  );

let errorMessage =
  fun
  | Failure(message) => message
  | error =>
    switch%platform () {
    | Native => Printexc.to_string(error)
    | Melange =>
      switch (error) {
      | Js.Exn.Error(error) =>
        Option.value(Js.Exn.message(error), ~default="Unknown error")
      | error => Printexc.to_string(error)
      }
    };

module ErrorText = {
  [@react.component]
  let make = (~children) =>
    <p className="text-red-400" role="alert"> {React.string(children)} </p>;
};

module NoteCard = {
  [@react.component]
  let make = (~id) => {
    let result = ReactQuery.useSuspenseQuery(noteQuery, id);
    let { note, loadedBy } = ReactQuery.SuspenseResult.data(result);
    let (title, setTitle) = RR.useStateValue("");
    let rename = ReactQuery.useMutation(renameMutation);
    let%melange_only onChangeTitle = event =>
      setTitle(React.Event.Form.target(event)##value);

    <article
      className="flex flex-col gap-2 rounded-lg border border-gray-600 p-4">
      <h2
        className={Cx.make([
          "text-xl font-bold",
          Theme.text(Theme.Color.Gray12),
        ])}>
        {React.string(note.title)}
      </h2>
      <p className={Theme.text(Theme.Color.Gray10)}>
        {React.string("Loaded by " ++ loadedBy)}
      </p>
      <button
        className="self-start rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold"
        onClick={_ => ignore(ReactQuery.SuspenseResult.refetch(result))}>
        {React.string(
           switch (ReactQuery.SuspenseResult.fetchStatus(result)) {
           | Fetching => "Refetching..."
           | Paused => "Waiting for the network..."
           | Idle => "Refetch on the client"
           },
         )}
      </button>
      {switch (ReactQuery.SuspenseResult.refetchError(result)) {
       | Some(error) =>
         <ErrorText>
           {"Refetch failed, showing the last data: " ++ errorMessage(error)}
         </ErrorText>
       | None => React.null
       }}
      <form
        className="flex flex-row gap-2"
        autoComplete="off"
        onSubmit=[%melange_only
          event => {
            React.Event.Form.preventDefault(event);
            ReactQuery.MutationResult.mutate(
              ~onSuccess=(_, _) => setTitle(""),
              rename,
              (id, title),
            );
          }
        ]>
        <InputText
          value=title
          onChange=onChangeTitle
          placeholder="New title"
        />
        <button
          type_="submit"
          className="rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold disabled:opacity-50"
          disabled={
            ReactQuery.MutationResult.isPending(rename) || title == ""
          }>
          {React.string(
             ReactQuery.MutationResult.isPending(rename)
               ? "Saving..." : "Rename",
           )}
        </button>
      </form>
      {switch (ReactQuery.MutationResult.status(rename)) {
       | ReactQuery.Failed(error, (_, attempted)) =>
         <div className="flex flex-row items-center gap-2">
           <ErrorText>
             {"Could not rename to \""
              ++ attempted
              ++ "\": "
              ++ errorMessage(error)}
           </ErrorText>
           <button
             className="rounded-md border border-gray-600 px-2 text-sm text-gray-300"
             onClick={_ => ReactQuery.MutationResult.reset(rename)}>
             {React.string("Dismiss")}
           </button>
         </div>
       | Idle
       | Running(_)
       | Succeeded(_) => React.null
       }}
    </article>;
  };
};

/* Every rename on the page, from any card. Empty on the server, so the
   browser hydrates the same markup. */
module Renames = {
  let describe =
    fun
    | ReactQuery.Running((id, title)) =>
      Some(Printf.sprintf("Saving note %d as \"%s\"...", id, title))
    | Succeeded(_, (id, title)) =>
      Some(Printf.sprintf("Renamed note %d to \"%s\"", id, title))
    | Failed(error, (id, title)) =>
      Some(
        Printf.sprintf(
          "Could not rename note %d to \"%s\": %s",
          id,
          title,
          errorMessage(error),
        ),
      )
    | Idle => None;

  [@react.component]
  let make = () => {
    let saving = ReactQuery.useIsMutating(~mutation=renameMutation, ());
    let renames =
      ReactQuery.useMutationState(renameMutation)
      |> Array.to_list
      |> List.filter_map(describe)
      |> List.rev;
    <section
      className="flex flex-col gap-1 rounded-lg border border-gray-700 p-4"
      ariaLabel="Renames">
      <h2 className={Theme.text(Theme.Color.Gray12)}>
        {React.string(
           saving == 0
             ? "Renames" : Printf.sprintf("Renames (%d saving)", saving),
         )}
      </h2>
      {switch (renames) {
       | [] =>
         <p className={Theme.text(Theme.Color.Gray10)}>
           {React.string("No renames yet")}
         </p>
       | renames =>
         <ul className={Theme.text(Theme.Color.Gray11)}>
           {renames
            |> List.filteri((index, _) => index < 5)
            |> List.mapi((index, line) =>
                 <li key={Int.to_string(index)}> {React.string(line)} </li>
               )
            |> Array.of_list
            |> React.array}
         </ul>
       }}
    </section>;
  };
};

/* Online on the server and while hydrating, then TanStack's online
   manager, which "Go offline" overrides until the browser reports a change
   or "Go online" is clicked. Offline, refetches pause until it is back. */
let useOnline = () =>
  React.useSyncExternalStoreWithServer(
    ~subscribe=onChange => ReactQuery.OnlineManager.subscribe(_ => onChange()),
    ~getSnapshot=ReactQuery.OnlineManager.isOnline,
    ~getServerSnapshot=() => true,
  );

/* Restoring on the server and while hydrating. Restored data older than
   the server's stays out of the cache. */
module RestoreStatus = {
  [@react.component]
  let make = () =>
    <p className={Theme.text(Theme.Color.Gray10)} ariaLabel="Restore">
      {React.string(
         ReactQuery.useIsRestoring()
           ? "Restoring the saved cache..."
           : "Saved cache restored. Every change is saved to localStorage.",
       )}
    </p>;
};

/* Zero on the server and while hydrating, then the live count. */
module NotesToolbar = {
  [@react.component]
  let make = () => {
    let client = ReactQuery.useQueryClient();
    let online = useOnline();
    let fetching =
      ReactQuery.useIsFetching(
        ~filter=ReactQuery.Filter.query(noteQuery),
        (),
      );
    <div className="flex flex-row items-center gap-3">
      <button
        className="rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold"
        onClick={_ =>
          ignore(
            ReactQuery.QueryClient.invalidate(
              client,
              ~refetch=All,
              ReactQuery.Filter.query(noteQuery),
            ),
          )
        }>
        {React.string("Refetch every note")}
      </button>
      <button
        className="rounded-md border border-gray-600 px-3 py-1 text-gray-300"
        onClick={_ => ReactQuery.OnlineManager.setOnline(!online)}>
        {React.string(online ? "Go offline" : "Go online")}
      </button>
      <p className={Theme.text(Theme.Color.Gray10)} role="status">
        {React.string(
           fetching == 0
             ? "No note is fetching"
             : Printf.sprintf(
                 "Fetching %d note%s...",
                 fetching,
                 fetching == 1 ? "" : "s",
               ),
         )}
      </p>
    </div>;
  };
};

/* The server fetches the first two pages, so they hydrate with
   [hasNextPage] already known. */
module NoteList = {
  [@react.component]
  let make = () => {
    let result = ReactQuery.useSuspenseInfiniteQuery(notesQuery, ());
    let { ReactQuery.pages, _ } =
      ReactQuery.SuspenseInfiniteResult.data(result);
    let hasNextPage = ReactQuery.SuspenseInfiniteResult.hasNextPage(result);
    let loadingMore =
      ReactQuery.SuspenseInfiniteResult.isFetchingNextPage(result);
    <section
      className="flex flex-col gap-2 rounded-lg border border-gray-700 p-4"
      ariaLabel="All notes">
      <h2 className={Theme.text(Theme.Color.Gray12)}>
        {React.string("All notes, one page at a time")}
      </h2>
      <ol
        className={Cx.make([
          "list-decimal pl-6",
          Theme.text(Theme.Color.Gray11),
        ])}>
        {pages
         |> Array.to_list
         |> List.concat_map(page => Array.to_list(page.notes))
         |> List.map(note =>
              <li key={Int.to_string(note.id)}>
                {React.string(note.title)}
              </li>
            )
         |> Array.of_list
         |> React.array}
      </ol>
      {switch (ReactQuery.SuspenseInfiniteResult.refetchError(result)) {
       | Some(error) =>
         <ErrorText>
           {"Could not load more: " ++ errorMessage(error)}
         </ErrorText>
       | None => React.null
       }}
      <button
        className="self-start rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold disabled:opacity-50"
        disabled={!hasNextPage || loadingMore}
        onClick={_ =>
          ignore(ReactQuery.SuspenseInfiniteResult.fetchNextPage(result))
        }>
        {React.string(
           loadingMore
             ? "Loading more..." : hasNextPage ? "Load more" : "No more notes",
         )}
      </button>
    </section>;
  };
};

/* One suspension for three notes, which all start loading at once. */
module Titles = {
  [@react.component]
  let make = (~ids) => {
    let notes =
      ReactQuery.useSuspenseQueries(
        ReactQuery.SuspenseQueries.all(
          Array.map(
            id => ReactQuery.SuspenseQueries.one(noteQuery, id),
            ids,
          ),
        ),
      );
    <section
      className="flex flex-col gap-1 rounded-lg border border-gray-700 p-4"
      ariaLabel="Titles">
      <h2 className={Theme.text(Theme.Color.Gray12)}>
        {React.string("Titles")}
      </h2>
      <ul className={Theme.text(Theme.Color.Gray11)}>
        {notes
         |> Array.map(result => {
              let { note, _ } = ReactQuery.SuspenseResult.data(result);
              <li key={Int.to_string(note.id)}>
                {React.string(Printf.sprintf("%d: %s", note.id, note.title))}
              </li>;
            })
         |> React.array}
      </ul>
    </section>;
  };
};

/* The server waits for the whole stream before showing it. Streaming again
   in the browser starts over, and each word shows as it arrives. */
module Sentence = {
  [@react.component]
  let make = () => {
    let result = ReactQuery.useSuspenseQuery(sentenceQuery, ());
    <section
      className="flex flex-col gap-2 rounded-lg border border-gray-700 p-4"
      ariaLabel="Streamed">
      <h2 className={Theme.text(Theme.Color.Gray12)}>
        {React.string("A streamed query")}
      </h2>
      <p className={Theme.text(Theme.Color.Gray11)}>
        {React.string(ReactQuery.SuspenseResult.data(result))}
      </p>
      <button
        className="self-start rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold"
        onClick={_ => ignore(ReactQuery.SuspenseResult.refetch(result))}>
        {React.string(
           ReactQuery.SuspenseResult.isFetching(result)
             ? "Streaming..." : "Stream again",
         )}
      </button>
    </section>;
  };
};

/* The devtools inline, with an error they can put on any query to show its
   error state. Closing the panel hides it again. */
module DevtoolsPanel = {
  let errorTypes = [|
    {
      ReactQueryDevtools.name: "Gone",
      error: query =>
        Failure(ReactQuery.CachedQuery.name(query) ++ " is gone (devtools)"),
    },
  |];

  [@react.component]
  let make = () => {
    let (shown, setShown) = RR.useStateValue(false);
    <div className="flex flex-col gap-2">
      <button
        className="self-start rounded-md border border-gray-600 px-3 py-1 text-gray-300"
        onClick={_ => setShown(!shown)}>
        {React.string(
           shown ? "Hide the devtools panel" : "Show the devtools panel",
         )}
      </button>
      {shown
         ? <div className="h-[400px]" ariaLabel="Devtools panel">
             <ReactQueryDevtools.Panel
               theme=Dark
               errorTypes
               onClose={() => setShown(false)}
             />
           </div>
         : React.null}
    </div>;
  };
};

module LoadError = {
  [@react.component]
  let make = (~id, ~error, ~retry) =>
    <article
      className="flex flex-col gap-2 rounded-lg border border-red-800 p-4">
      <ErrorText>
        {"Could not load note "
         ++ Int.to_string(id)
         ++ ": "
         ++ errorMessage(error)}
      </ErrorText>
      <button
        className="self-start rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold"
        onClick={_ => retry()}>
        {React.string("Retry")}
      </button>
    </article>;
};

module Loading = {
  [@react.component]
  let make = (~id) =>
    <p className={Theme.text(Theme.Color.Gray10)}>
      {React.string("Loading note " ++ Int.to_string(id) ++ "...")}
    </p>;
};

module NoteSlot = {
  [@react.component]
  let make = (~id) =>
    <ReactQuery.QueryErrorResetBoundary>
      <ReactQuery.ErrorBoundary
        fallback={(~error, ~retry) => <LoadError id error retry />}>
        <React.Suspense fallback={<Loading id />}>
          <NoteCard id />
        </React.Suspense>
      </ReactQuery.ErrorBoundary>
    </ReactQuery.QueryErrorResetBoundary>;
};

/* Renders nothing on the server. The note was prefetched, so a click shows it
   from the cache without a request. */
module Peek = {
  [@react.component]
  let make = (~id) => {
    let (shown, setShown) = RR.useStateValue(false);
    <div className="flex flex-col gap-4">
      <button
        className="self-start rounded-md bg-yellow-600 px-3 py-1 text-slate-900 font-semibold"
        onClick=[%melange_only _ => setShown(!shown)]>
        {React.string(
           (shown ? "Hide" : "Show") ++ " note " ++ Int.to_string(id),
         )}
      </button>
      {shown ? <NoteSlot id /> : React.null}
    </div>;
  };
};

/* [loaderState] is the request handler's dehydrated loader client, which
   the page also embeds for the browser's [HydrationBoundary]. */
[@react.component]
let make = (~loaderState) => {
  ReactQuery.usePrefetchInfiniteQuery(~pages=2, notesQuery, ());
  <DemoLayout background=Theme.Color.Gray2>
    <main className="flex flex-col gap-6 mt-4 w-full max-w-[800px]">
      <h1
        className={Cx.make([
          "text-4xl font-bold",
          Theme.text(Theme.Color.Gray12),
        ])}>
        {React.string("SSR with TanStack Query")}
      </h1>
      <p className={Theme.text(Theme.Color.Gray11)}>
        {React.string(
           "Each note is a TanStack query with one query function for the server and the browser. The request handler prefetches every note before rendering, and the page prefetches the note list with usePrefetchInfiniteQuery while it renders; each one streams to the browser as it resolves, ahead of the Suspense boundary that shows it, so the browser hydrates without fetching again. Note 0 comes from a separate loader client: the handler dehydrates it, and a HydrationBoundary hydrates it on the server and, from the embedded state, in the browser, so showing it after a click needs no fetch. The titles of notes 1 to 3 come from one useSuspenseQueries call. Note 4 does not exist, so its query fails on the server and again in the browser, where an error boundary shows the error; each note has its own QueryErrorResetBoundary, so Retry only resets that note. The sentence is a streamed query whose words arrive one at a time. Go offline, and refetches wait for the network until you go online again. Renames are optimistic: the new title shows at once, and goes back when the server rejects it, as it does for titles longer than 40 characters.",
         )}
      </p>
      <React.Suspense
        fallback={
          <p className={Theme.text(Theme.Color.Gray10)}>
            {React.string("Loading the note list...")}
          </p>
        }>
        <NoteList />
      </React.Suspense>
      <NotesToolbar />
      <RestoreStatus />
      <Renames />
      <React.Suspense
        fallback={
          <p className={Theme.text(Theme.Color.Gray10)}>
            {React.string("Loading titles...")}
          </p>
        }>
        <Titles ids=titleIds />
      </React.Suspense>
      <React.Suspense
        fallback={
          <p className={Theme.text(Theme.Color.Gray10)}>
            {React.string("Streaming the sentence...")}
          </p>
        }>
        <Sentence />
      </React.Suspense>
      <ReactQuery.HydrationBoundary state=loaderState>
        <Peek id=peekId />
      </ReactQuery.HydrationBoundary>
      {noteIds
       |> Array.map(id => <NoteSlot key={Int.to_string(id)} id />)
       |> React.array}
    </main>
    <DevtoolsPanel />
    <ReactQueryDevtools buttonPosition=BottomLeft />
  </DemoLayout>;
};
