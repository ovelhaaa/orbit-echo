1. Modify `ensurePreviewGraph` in `targets/wasm/js/demo.js` to create a `ScriptProcessorNode` instead of an offline `wetElement`.
2. Map `onaudioprocess` on the script node to pass audio blocks to the WASM `api.process` function.
3. Clean up references to `wetElement` (or safely ignore them by keeping `previewGraph.wetElement = null`).
4. Reconnect the audio graph: `drySource -> effectNode -> wetGain -> destination`.
5. Ensure `processStereoBuffer` call is followed by `api.reset` so the real-time processor starts with clean DSP state.
6. Verify no regressions on offline processing/download functionality.
7. Run `pre_commit_instructions` tool to verify checklist.
8. Submit.
