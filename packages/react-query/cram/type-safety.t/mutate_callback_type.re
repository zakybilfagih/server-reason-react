open ReactQuery;

let renameNow = () => {
  let rename = useMutation(Fixtures.rename);
  MutationResult.mutate(
    ~onSuccess=(title, _) => print_endline(title),
    rename,
    (1, "title"),
  );
};
