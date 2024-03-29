import { LitElement, html, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { map } from 'lit/directives/map.js';
import '@material/web/autocomplete/outlined-autocomplete.js';
import '@material/web/autocomplete/autocomplete-item.js';

import { ContextConsumer } from '@lit-labs/context';
import { hasiContext, PlotParameter } from './state/hasi.machine.js';
import { fields } from './scan.types.js';

@customElement('biomarker-picker')
export class BiomarkerPicker extends LitElement {
  @property() parameter!: PlotParameter;

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  private _setPlotParameter = (e: CustomEvent) => {
    this.stateService.value?.service.send({
      type: 'PLOT_PARAMETER_CHANGED',
      parameter: this.parameter,
      value: e.detail.value,
    });
    e.stopPropagation();
  };

  render() {
    return html`
      <md-outlined-autocomplete
        label=${this.parameter}
        @autocomplete-value-changed=${this._setPlotParameter}
      >
        ${map(
          fields,
          (field) =>
            html`<md-autocomplete-item
              headline="${field}"
            ></md-autocomplete-item>`
        )}
      </md-outlined-autocomplete>
    `;
  }

  static styles = css`
    :host {
      display: grid;
      margin: 0.4rem;

      position: relative;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'biomarker-picker': BiomarkerPicker;
  }
}
