/* The [usersView] search param: a JSON object in the URL, like TanStack's
   search serializer writes it. Fields keep the order they were set in. */

type sortBy =
  | Name
  | Id
  | Email;

type field =
  | SortBy(sortBy)
  | FilterBy(string);

type t = list(field);

let sortByToString =
  fun
  | Name => "name"
  | Id => "id"
  | Email => "email";

let sortByOfString =
  fun
  | "name" => Some(Name)
  | "id" => Some(Id)
  | "email" => Some(Email)
  | _ => None;

let sortBy = view =>
  List.find_map(
    fun
    | SortBy(sortBy) => Some(sortBy)
    | FilterBy(_) => None,
    view,
  );

let filterBy = view =>
  List.find_map(
    fun
    | FilterBy(filterBy) => Some(filterBy)
    | SortBy(_) => None,
    view,
  );

let sameKey = (a, b) =>
  switch (a, b) {
  | (SortBy(_), SortBy(_))
  | (FilterBy(_), FilterBy(_)) => true
  | _ => false
  };

/* [{...view, [key]: value}]: an existing key keeps its place. */
let set = (view, field) =>
  List.exists(sameKey(field), view)
    ? List.map(
        existing => sameKey(field, existing) ? field : existing,
        view,
      )
    : view @ [field];

let setSortBy = (view, sortBy) => set(view, SortBy(sortBy));

/* [{...view, filterBy: value || undefined}] */
let setFilterBy = (view, filterBy) =>
  switch (filterBy) {
  | "" => List.filter(field => !sameKey(field, FilterBy("")), view)
  | filterBy => set(view, FilterBy(filterBy))
  };

let jsonString = value => {
  let buffer = Buffer.create(String.length(value) + 2);
  Buffer.add_char(buffer, '"');
  String.iter(
    character =>
      switch (character) {
      | '"' => Buffer.add_string(buffer, "\\\"")
      | '\\' => Buffer.add_string(buffer, "\\\\")
      | '\n' => Buffer.add_string(buffer, "\\n")
      | '\r' => Buffer.add_string(buffer, "\\r")
      | '\t' => Buffer.add_string(buffer, "\\t")
      | '\b' => Buffer.add_string(buffer, "\\b")
      | '\012' => Buffer.add_string(buffer, "\\f")
      | character when Char.code(character) < 0x20 =>
        Buffer.add_string(
          buffer,
          Printf.sprintf("\\u%04x", Char.code(character)),
        )
      | character => Buffer.add_char(buffer, character)
      },
    value,
  );
  Buffer.add_char(buffer, '"');
  Buffer.contents(buffer);
};

let to_string = view =>
  "{"
  ++ String.concat(
       ",",
       List.map(
         fun
         | SortBy(sortBy) =>
           "\"sortBy\":" ++ jsonString(sortByToString(sortBy))
         | FilterBy(filterBy) => "\"filterBy\":" ++ jsonString(filterBy),
         view,
       ),
     )
  ++ "}";

let field = ((key, value)) =>
  switch (key, value) {
  | ("sortBy", Some(value)) =>
    switch (sortByOfString(value)) {
    | Some(sortBy) => Ok(Some(SortBy(sortBy)))
    | None => Error("sortBy must be name, id or email")
    }
  | ("filterBy", Some(value)) => Ok(Some(FilterBy(value)))
  | ("sortBy" | "filterBy", None) => Error(key ++ " must be a string")
  | _ => Ok(None)
  };

let fields = pairs =>
  List.fold_left(
    (result, pair) =>
      switch (result, field(pair)) {
      | (Ok(view), Ok(Some(field))) => Ok(view @ [field])
      | (Ok(view), Ok(None)) => Ok(view)
      | (Error(error), _)
      | (_, Error(error)) => Error(error)
      },
    Ok([]),
    pairs,
  );

let parse = value =>
  switch%platform () {
  | Native =>
    switch (Yojson.Basic.from_string(value)) {
    | `Assoc(pairs) =>
      fields(
        List.map(
          ((key, value)) =>
            (
              key,
              switch (value) {
              | `String(value) => Some(value)
              | _ => None
              },
            ),
          pairs,
        ),
      )
    | _ => Error("usersView must be an object")
    | exception _ => Error("usersView must be JSON")
    }
  | Melange =>
    switch (Js.Json.decodeObject(Js.Json.parseExn(value))) {
    | Some(dict) =>
      fields(
        Array.to_list(
          Array.map(
            ((key, value)) => (key, Js.Json.decodeString(value)),
            Js.Dict.entries(dict),
          ),
        ),
      )
    | None => Error("usersView must be an object")
    | exception _ => Error("usersView must be JSON")
    }
  };
