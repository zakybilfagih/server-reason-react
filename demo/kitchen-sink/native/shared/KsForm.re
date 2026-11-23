/* new FormData(event.target).get(name) */
let%melange_only get = (event: React.Event.Form.t, name: string): string => {
  let get: (React.Event.Form.t, string) => string = [%mel.raw
    {js|(event, name) => { const value = new FormData(event.target).get(name); return value == null ? "" : String(value); }|js}
  ];
  get(event, name);
};

let%melange_only setCookie = (name: string, value: option(string)) => {
  let write: (string, string, bool) => unit = [%mel.raw
    {js|(name, value, remove) => { document.cookie = name + "=" + encodeURIComponent(value) + "; path=/; SameSite=Lax" + (remove ? "; max-age=0" : ""); }|js}
  ];
  write(name, Option.value(~default="", value), Option.is_none(value));
};

let submitClass = "bg-blue-500 rounded-sm p-2 uppercase text-white font-black disabled:opacity-50";

let bounce = color =>
  "inline-block px-2 py-1 rounded-sm "
  ++ color
  ++ " text-white animate-bounce [animation-iteration-count:2.5] [animation-duration:.3s]";
