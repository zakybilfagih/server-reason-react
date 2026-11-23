open ReactQuery;

let title = () =>
  switch (QueryResult.status(useQuery(Fixtures.note, 1))) {
  | Success(title) => title
  | Pending => "loading"
  };
