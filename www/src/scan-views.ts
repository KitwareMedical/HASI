import { ContextConsumer } from "@lit-labs/context";
import { LitElement, css, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { repeat } from "lit/directives/repeat.js";
import { EventObject } from "xstate";
import { ScanId } from "./scan.types";
import { hasiContext } from "./state/hasi.machine";

@customElement("scan-views")
export class ScanViews extends LitElement {
  @property() scanIds: ScanId[] = [];

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  render() {
    return html`
      ${repeat(
        this.scanIds,
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

  scanClickedHandler = (e: EventObject) => {
    if (e.type === "SCAN_CLICKED")
      this.scanIds = Array.from(
        this.stateService.value!.service.machine.context.selectedScans
      );
  };

  connectedCallback() {
    super.connectedCallback();
    this.stateService.value!.service.onEvent(this.scanClickedHandler);
  }

  disconnectedCallback() {
    this.stateService.value!.service.off(this.scanClickedHandler);
    super.disconnectedCallback();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "scan-views": ScanViews;
  }
}
