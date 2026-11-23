open ReactQuery;

let total = () =>
  switch (InfiniteResult.status(useInfiniteQuery(Fixtures.feed, "me"))) {
  | Success({ pages, _ }) => Array.fold_left((+), 0, pages)
  | Pending
  | Error(_) => 0
  };
