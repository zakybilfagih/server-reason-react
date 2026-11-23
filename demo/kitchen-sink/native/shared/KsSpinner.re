[@react.component]
let make = (~show=true, ~wait="delay-300") =>
  <div
    className={
      "inline-block animate-spin px-3 transition "
      ++ (
        show
          ? "opacity-100 duration-500 " ++ wait
          : "duration-500 opacity-0 delay-0"
      )
    }>
    {React.string({js|⍥|js})}
  </div>;
