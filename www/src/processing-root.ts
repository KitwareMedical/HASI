import { LitElement, html } from 'lit';
import { customElement } from 'lit/decorators.js';
import { literal } from 'lit/static-html.js';

import './accordion-layout.js';
import './scan-table.js';
import './upload-scans.js';

const SECTIONS = [
  { title: 'Scan Table', tag: literal`scan-table` },
  { title: 'Upload Scans', tag: literal`upload-scans` },
];

@customElement('processing-root')
export class ProcessingRoot extends LitElement {
  render() {
    return html` <accordion-layout .sections=${SECTIONS}></accordion-layout> `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'processing-root': ProcessingRoot;
  }
}
