// Copyright (c) YugaByte, Inc.

import React, { Component } from 'react';
import { Col } from 'react-bootstrap';
import DescriptionItem from '../DescriptionItem';
import {isValidArray} from '../../utils/ObjectUtils';
import {FormattedNumber} from 'react-intl';
import moment from 'moment';

class StatsPanelComponent extends Component {
  render() {
    const {value, label} = this.props;
    return (
      <Col md={4} className="tile_stats_count text-center">
        <DescriptionItem>
          <div className="count">
            {value}
          </div>
        </DescriptionItem>
        <span className="count_top">
          {label}
        </span>
      </Col>
    )
  }
}
export default class HighlightedStatsPanel extends Component {

  render() {
    const { universe: { universeList, loading } } = this.props;
    if (loading) {
      return <div className="container">Loading...</div>;
    }
    var numNodes = 0;
    var totalCost = 0;
    if (isValidArray(universeList)) {
      universeList.forEach(function (universeItem) {
        numNodes += universeItem.universeDetails.userIntent.numNodes;
        totalCost += universeItem.pricePerHour * 24 * moment().daysInMonth();
      });
    }
    return (
      <div className="row tile_count universe-cost-panel-container">
        <Col md={6} mdOffset={3}>
          <StatsPanelComponent value={universeList.length} label={"Universes"}/>
          <StatsPanelComponent value={numNodes} label={"Nodes"}/>
          <StatsPanelComponent value={<FormattedNumber value={totalCost} maximumFractionDigits={2}
                                                       style="currency" currency="USD"/>} label={"Cost"}/>
        </Col>
      </div>
    )
  }
}
