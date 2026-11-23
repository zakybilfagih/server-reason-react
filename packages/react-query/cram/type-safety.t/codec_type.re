open ReactQuery;

let count =
  Query.make(
    ~name="count",
    ~key=() => [],
    ~codec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~fetch=(~signal as _, ()) => Lwt.return(1),
    (),
  );
