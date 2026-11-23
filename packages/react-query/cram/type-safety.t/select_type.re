open ReactQuery;

let shout = () =>
  switch (QueryResult.status(useQuerySelect(~select=String.length, Fixtures.note, 1))) {
  | Success(title) => title ++ "!"
  | Pending
  | Error(_) => ""
  };
