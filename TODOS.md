# TODOS

- [x] Create a basic reason project with alcotest
- [x] Take a look at a [Rust implementation](https://github.com/MaibornWolff/react-wasm-dom)
- [x] Try to render a string given a React Tree
- [x] Define the React Tree using React.createElement (not the JSX ppx)
- [x] Allow attributes to be strings or booleans
- [x] Handle ReasonReact APIS. React.null, React.string, React.int
- [x] Handle fragments
- [x] Add data-reactroot="" to the root element
  - [x] Abstract renderToStringRec to handle internal state (ref is_root)
- [x] Handle value/defaultValue logic and similars
- [x] Handle [style attribute](https://github.com/MaibornWolff/react-wasm-dom/blob/main/src/__tests__/CSSPropertyOperations-test.jsx)
- [x] Implement [React.Context](https://github.com/preactjs/preact-render-to-string/blob/master/test/context.test.js)
- [x] Does the order of attributes matter on cloneElement
- [x] React.cloneElement
  - How does it work for Fragments/Texts/Empty?
- [x] [Scape text with HTML](https://github.com/MaibornWolff/react-wasm-dom/blob/main/src/__tests__/escapeTextForBrowser-test.jsx) [entities](https://stackoverflow.com/questions/7381974/which-characters-need-to-be-escaped-in-html)
  - Should we handle every html entity?
- [x] Handle refs
- [x] Handle useContext
- [x] Run useState
- [x] Handle React.memoN
- [x] Handle React.useCallbackN
- [x] Ignore useEffect
- [x] Handle React dengerouslySetInnerHTML
- [x] Add test for hooks
- [ ] Implement all JSX logic
  - [ReactDOMServerIntegrationAttributes](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationAttributes-test.js)
  - [ReactDOMServerIntegrationCheckbox](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationCheckbox-test.js)
  - [ReactDOMServerIntegrationElements](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationElements-test.js)
  - [ReactDOMServerIntegrationFragment](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationFragment-test.js)
  - [ReactDOMServerIntegrationHooks](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationHooks-test.js)
  - [ReactDOMServerIntegrationInput](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationInput-test.js)
  - [ReactDOMServerIntegrationLegacyContext](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationLegacyContext-test.js)
  - [ReactDOMServerIntegrationLegacyContextDisabled](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationLegacyContextDisabled-test.internal.js)
  - [ReactDOMServerIntegrationModes](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationModes-test.js)
  - [ReactDOMServerIntegrationNewContext](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationNewContext-test.js)
  - [ReactDOMServerIntegrationReconnecting](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationReconnecting-test.js)
  - [ReactDOMServerIntegrationRefs](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationRefs-test.js)
  - [ReactDOMServerIntegrationSelect](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationSelect-test.js)
  - [ReactDOMServerIntegrationSpecialTypes](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationSpecialTypes-test.js)
  - [ReactDOMServerIntegrationTextarea](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationTextarea-test.js)
  - [ReactDOMServerIntegrationUntrustedURL](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationUntrustedURL-test.internal.js)
  - [ReactDOMServerIntegrationUserInteraction](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerIntegrationUserInteraction-test.js)
  - [ReactDOMServerLifecycles](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerLifecycles-test.js)
  - [ReactDOMServerPartialHydration](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerPartialHydration-test.internal.js)
  - [ReactDOMServerSelectiveHydration](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerSelectiveHydration-test.internal.js)
  - [ReactDOMServerSuspense](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMServerSuspense-test.internal.js)
  - [x] cased custom attributes
  - [x] no HTML events
  - [x] no unknown events
- [ ] Implement renderToString
  - Check what are the differences
  - Import test cases and implement those
- [ ] Implement the rest of the React API
  - [x] Support forwardRef
  - [x] Support Ref
  - [ ] Suspense
  - [ ] useImperativeHandle
  - [ ] useLayoutEffect
  - [ ] useTransition
  - [ ] setDisplayName
  - [ ] displayName
- [x] ReactDOM.Style.make
- [ ] Handle unicode. Add Uutfs?
- [ ] Implement dispatcher
- [ ] Add setState callbacks as tick on the dispatcher
- [ ] Add support for SVGs (they have a few differences in rendering the tag and attributes)
- [x] Rename React.Node.t and others to React.Element
- [ ] Ensure types from TypeScript make sense with our implementation
- [ ] A way to trigger warnings for invalid attributes in special elements
  - [ReactDOMTextarea](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMTextarea-test.js)
  - [ReactDOMSelect](https://github.com/facebook/react/blob/main/packages/react-dom/src/__tests__/ReactDOMSelect-test.js)
- [ ] Re-visit cloneElement

## Demo

- [x] Create a server to render HTML with native-react-dom
- [x] Compile the same code to the client

## ppx transformation

- [x] Remove all `jsoo` specific
- [x] Render to something that makes sense
- [x] Transform attributes to JSX
- [x] Transform lower case components
- [x] Transform React.Components
- [ ] Enable reason test from ppx
- [ ] Add Events
- [ ] Transform signatures
- [ ] Transform externals
  - (Lident "React", "componentLike")
- [ ] Allow recursive components?
- [x] Transform Fragments to the right type s/React.Fragment.createElement/React.createFragment?
- [x] Is childrenArg ref hack, necessary?
- [ ] Can we cleanup this fucking beast?

## Questions

- How should we handle errors from `createElement` or `renderToString`
- Suspense?
  "ReactDOMServer does not yet support Suspense - server/node_modules/react-dom/cjs/react-dom-server.node.development.js:3518"
- How does SSR handle component runtime?
  - If there is a function call such as state` inside a component?
  - Lists with keys, why SSR complains?
    - Because there's re-rendering inside SSR. Reconciling? Commiting? What?
- Do we need CSSOperations?
  - Add the units (adding `px` when matters and other cases from [CSSPropertyOperations-test](https://github.com/MaibornWolff/react-wasm-dom/blob/main/src/__tests__/CSSPropertyOperations-test.jsx))?
- How difficult would be to support Server components?
- Do we need to support [React.Children API](https://github.com/reasonml/reason-react/blob/master/src/React.re#L58-L76) from reason-react

### Not native-react-dom related

- How we are going to mock the DOM API?
- Do we have any way to ensure `bs.obj` compiles in native?
  - Probably only in melange?
  - Add ppx processing to replace `bs.obj` with classic OCaml objects (e.g. `{"foo": 2}` translates to `object method foo = 2 end`).