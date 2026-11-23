open ReactQuery;

let saving = () =>
  useMutationState(Fixtures.rename)
  |> Array.map(
       fun
       | Running(title) => title ++ "..."
       | Idle
       | Failed(_)
       | Succeeded(_) => "",
     );
