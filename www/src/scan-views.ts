import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";
import { repeat } from "lit/directives/repeat.js";
import { connectState } from "./utils/SelectState";

@customElement("scan-views")
export class ScanViews extends LitElement {
  scanIdsController = connectState(
    this,
    (state) => [...state.context.selectedScans],
    (oldScans, newScans) =>
      oldScans.length === newScans.length &&
      oldScans.every((id) => newScans.includes(id))
  );

  render() {
    return html`
      ${repeat(
        this.scanIdsController.selected() ?? [],
        (viewId) => viewId,
        (viewId) => html`<div>${viewId}</div>`
      )}
    `;
  }

  static styles = css`
    :host {
      display: flex;
    }

    :host > * {
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "scan-views": ScanViews;
  }
}
