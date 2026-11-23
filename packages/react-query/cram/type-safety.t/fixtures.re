open ReactQuery;

let note =
  Query.make(
    ~name="note",
    ~key=(id: int) => [Key.int(id)],
    ~codec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~fetch=(~signal as _, id) => Lwt.return(Int.to_string(id)),
    (),
  );

let rename =
  Mutation.make(
    ~name="rename",
    ~fn=((id: int, title: string)) => Lwt.return(id + String.length(title)),
    (),
  );

let feed =
  InfiniteQuery.make(
    ~name="feed",
    ~key=(user: string) => [Key.string(user)],
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
    ~fetchPage=(~signal as _, _, cursor) => Lwt.return(Int.to_string(cursor)),
    ~getNextPageParam=_ => None,
    (),
  );
