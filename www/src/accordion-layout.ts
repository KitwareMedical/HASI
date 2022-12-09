import { LitElement, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { StaticValue, html } from 'lit/static-html.js';
import { repeat } from 'lit/directives/repeat.js';
import '@shoelace-style/shoelace/dist/components/details/details.js';
import { setDefaultAnimation } from '@shoelace-style/shoelace/dist/utilities/animation-registry.js';

export type Section = {
  title: string;
  tag: StaticValue;
};

setDefaultAnimation('details.show', {
  keyframes: [{ opacity: '0' }, { opacity: '1' }],
  options: {
    duration: 200,
  },
});
setDefaultAnimation('details.hide', null);

@customElement('accordion-layout')
export class AccordionLayout extends LitElement {
  @property() sections: Array<Section> = [];

  protected firstUpdated(): void {
    this.renderRoot.querySelectorAll('sl-details').forEach((details) => {
      var sheet = new CSSStyleSheet();
      sheet.replaceSync(`.details__body { flex: 1;
       display: flex;
       flex-direction: column;
      }`);
      details!.shadowRoot!.adoptedStyleSheets = [
        ...details!.shadowRoot!.adoptedStyleSheets,
        sheet,
      ];
    });
  }

  render() {
    return html`
      <div class="main-layout">
        ${repeat(
          this.sections,
          ({ title }) => title,
          ({ title, tag }) => html`
            <sl-details summary="${title}" open>
              <${tag}></${tag}>
            </sl-details>
          `
        )}
      </div>
    `;
  }

  static styles = css`
    :host {
      height: 100%;
    }

    .main-layout {
      height: 100%;
      display: flex;
      flex-direction: column;
    }

    .main-layout > * {
      flex: 1 1 20rem;
      display: flex;
      flex-direction: column;
    }

    .main-layout > *::part(base) {
      flex: 1;
      display: flex;
      flex-direction: column;
      border: none;
      border-radius: 0;
    }

    .main-layout > *::part(content) {
      flex: 1;
      display: flex;
      flex-direction: column;
      padding: 0;
    }

    .main-layout > * > * {
      flex: 1;
    }

    sl-details:not([open]) {
      flex: 0;
      margin: 0 0 2px 0;
    }

    sl-details::part(header) {
      background-color: var(--primary-color);
      color: var(--text-white);
      font-size: 22px;
      padding: 0.6rem 0.2rem;
      border-radius: 0;
    }

    sl-details::part(summary) {
      justify-content: center;
    }

    sl-details::part(summary-icon) {
      margin-right: 0.6rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'accordion-layout': AccordionLayout;
  }
}
