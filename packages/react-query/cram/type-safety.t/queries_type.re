open ReactQuery;

let total = () => {
  let (first, second) =
    useSuspenseQueries(
      SuspenseQueries.both(SuspenseQueries.one(Fixtures.note, 1), SuspenseQueries.one(Fixtures.note, 2)),
    );
  SuspenseResult.data(first) + SuspenseResult.data(second);
};
