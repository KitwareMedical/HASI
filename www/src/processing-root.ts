import { LitElement, css, html } from 'lit';
import { customElement } from 'lit/decorators.js';

@customElement('processing-root')
export class ProcessingRoot extends LitElement {
  render() {
    return html` <h2>Processing</h2> `;
  }
  static styles = css`
    :host {
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'processing-root': ProcessingRoot;
  }
}
