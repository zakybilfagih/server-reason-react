let () =
  print_endline(
    Yojson.Basic.pretty_to_string(
      `List(
        Array.to_list(
          Array.map(line => `String(line), ManagersScenario.run()),
        ),
      ),
    ),
  );
