open ReactQuery;

let title = () =>
  switch (useQuery(Fixtures.note, 1)) {
  | Success(title) => title
  | _ => ""
  };
