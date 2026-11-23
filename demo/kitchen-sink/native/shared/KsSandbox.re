/* The example's sandbox panel. */
let button = "bg-blue-500 text-white rounded-sm p-1 px-2";

module Slider = {
  [@react.component]
  let make = (~label, ~value: int, ~onChange: int => unit) => {
    let%melange_only onChange = event =>
      onChange(int_of_float(React.Event.Form.target(event)##valueAsNumber));
    <div>
      <div>
        {React.string(label ++ ": " ++ Int.to_string(value) ++ "ms")}
      </div>
      <input
        type_="range"
        min="0"
        max="5000"
        step=?{
          switch%platform () {
          | Native => Some("100")
          | Melange => Some(100.)
          }
        }
        value={Int.to_string(value)}
        onChange
        className="w-full"
      />
    </div>;
  };
};

[@react.component]
let make = () => {
  let settings = KsSettings.use();
  <div
    className="text-xs fixed w-52 shadow-md shadow-black/20 rounded-sm bottom-2 left-2 bg-white dark:bg-gray-800 bg-opacity-75 border-b flex flex-col gap-1 flex-wrap items-left divide-y">
    <div className="p-2 space-y-2">
      <div className="flex gap-2">
        <button
          className=button
          onClick=[%melange_only _ => settings.setLoaderDelay(150)]>
          {React.string("Fast")}
        </button>
        <button
          className=button
          onClick=[%melange_only _ => settings.setLoaderDelay(500)]>
          {React.string("Fast 3G")}
        </button>
        <button
          className=button
          onClick=[%melange_only _ => settings.setLoaderDelay(2000)]>
          {React.string("Slow 3G")}
        </button>
      </div>
      <Slider
        label="Loader Delay"
        value={settings.loaderDelay}
        onChange={settings.setLoaderDelay}
      />
    </div>
    <div className="p-2 space-y-2">
      <div className="flex gap-2">
        <button
          className=button
          onClick=[%melange_only
            _ => {
              settings.setPendingMs(1000);
              settings.setPendingMinMs(500);
            }
          ]>
          {React.string("Reset to Default")}
        </button>
      </div>
      <Slider
        label="defaultPendingMs"
        value={settings.pendingMs}
        onChange={settings.setPendingMs}
      />
      <Slider
        label="defaultPendingMinMs"
        value={settings.pendingMinMs}
        onChange={settings.setPendingMinMs}
      />
    </div>
  </div>;
};
