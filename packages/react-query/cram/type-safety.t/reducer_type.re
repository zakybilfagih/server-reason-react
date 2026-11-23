open ReactQuery;

let counts =
  StreamedQuery.make(
    ~name="counts",
    ~key=() => [],
    ~codec=
      Codec.make(
        ~encode=Melange_json.Primitives.int_to_json,
        ~decode=Melange_json.Primitives.int_of_json,
      ),
    ~stream=(~signal as _, ()) => Lwt.return(Stream.fromArray([|"one"|])),
    ~reducer=(total, chunk) => total + chunk,
    ~initialValue=0,
    (),
  );
