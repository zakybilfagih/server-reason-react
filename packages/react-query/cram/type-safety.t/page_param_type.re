open ReactQuery;

let feed =
  InfiniteQuery.make(
    ~name="feed",
    ~key=() => [],
    ~pageCodec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~paramCodec=
      Codec.make(
        ~encode=Melange_json.Primitives.int_to_json,
        ~decode=Melange_json.Primitives.int_of_json,
      ),
    ~initialPageParam=0,
    ~fetchPage=(~signal as _, (), cursor) => Lwt.return(Int.to_string(cursor)),
    ~getNextPageParam=_ => Some("next"),
    (),
  );
