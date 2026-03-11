'use strict';
const vscode = require('vscode');

function activate(context) {
  // Register the debug adapter descriptor factory so VS Code
  // uses the adapter script declared in package.json automatically.
  context.subscriptions.push(
    vscode.debug.registerDebugAdapterDescriptorFactory(
      'vexium',
      new VexiumAdapterFactory()
    )
  );
}

class VexiumAdapterFactory {
  createDebugAdapterDescriptor(session, executable) {
    // `executable` is already configured from package.json
    // (node + ./src/debugAdapter.js) — just return it.
    return executable;
  }
}

function deactivate() {}

module.exports = { activate, deactivate };
