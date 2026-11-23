open ReactQuery;

let rename =
  Mutation.make(
    ~name="rename",
    ~fn=(title: string) => Lwt.return(String.length(title)),
    ~onMutate=(_, _) => Lwt.return(42),
    ~onError=(_, _, _, previous) => Lwt.return(ignore(Option.map(String.length, previous))),
    (),
  );
