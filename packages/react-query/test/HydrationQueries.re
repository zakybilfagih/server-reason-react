/* The queries of hydration_stream.json, so [hydrateFromServer] can decode
   them. The browser never fetches them in the tests. */
open ReactQuery;

let string =
  Codec.make(
    ~encode=Melange_json.Primitives.string_to_json,
    ~decode=Melange_json.Primitives.string_of_json,
  );

let noFetch = _ => Js.Promise.reject(Failure("the test fetches it itself"));

let queries =
  [
    "resolved",
    "pending",
    "stale",
    "failing",
    "retried",
    "boundary",
    "existing",
    "in-flight",
    "outlives",
    "render-prefetch",
  ]
  |> List.map(name =>
       Query.make(
         ~name,
         ~key=(id: int) => [Key.int(id)],
         ~codec=string,
         ~fetch=(~signal as _, id) => noFetch(id),
         (),
       )
     );

let streamed =
  StreamedQuery.make(
    ~name="streamed",
    ~key=(id: int) => [Key.int(id)],
    ~codec=string,
    ~stream=(~signal as _, id) => noFetch(id),
    ~reducer=(++),
    ~initialValue="",
    (),
  );

let infinite =
  InfiniteQuery.make(
    ~name="infinite",
    ~key=() => [],
    ~pageCodec=string,
    ~paramCodec=
      Codec.make(
        ~encode=Melange_json.Primitives.int_to_json,
        ~decode=Melange_json.Primitives.int_of_json,
      ),
    ~initialPageParam=0,
    ~fetchPage=(~signal as _, (), param) => noFetch(param),
    ~getNextPageParam=_ => None,
    (),
  );

let hydrateFromServer = hydrateFromServer;
