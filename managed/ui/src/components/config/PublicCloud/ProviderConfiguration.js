// Copyright (c) YugaByte, Inc.

import React, { Component } from 'react';
import { getPromiseState } from 'utils/PromiseUtils';
import { isNonEmptyObject, isDefinedNotNull, isNonEmptyArray, isNonEmptyString } from 'utils/ObjectUtils';
import { withRouter } from 'react-router';
import _ from 'lodash';
import ProviderResultView from './views/ProviderResultView';
import ProviderBootstrapView from './views/ProviderBootstrapView';
import AWSProviderInitView from './views/AWSProviderInitView';
import GCPProviderInitView from './views/GCPProviderInitView';
import { YBLoading } from '../../common/indicators';

class ProviderConfiguration extends Component {
  constructor(props) {
    super(props);
    this.state = {
      currentView: '',
      currentTaskUUID: '',
      refreshSucceeded: false
    };
    this.getResultView = this.getResultView.bind(this);
    this.getBootstrapView = this.getBootstrapView.bind(this);
    this.getInitView = this.getInitView.bind(this);
  }

  componentWillMount() {
    this.props.fetchCustomerTasksList();
  }

  getInitView() {
    const {providerType} = this.props;
    if (providerType === "gcp") {
      return <GCPProviderInitView {...this.props}/>;
    } else if (providerType === "aws") {
      return <AWSProviderInitView {...this.props}/>;
    }
  }

  componentDidMount() {
    const {configuredProviders, tasks: {customerTaskList}, providerType, getCurrentTaskData} = this.props;
    const currentProvider = configuredProviders.data.find((provider) => provider.code === providerType);
    if (getPromiseState(configuredProviders).isLoading() || getPromiseState(configuredProviders).isInit()) {
      this.setState({currentView: 'loading'});
    } else {
      this.setState({currentView: isNonEmptyObject(currentProvider) ? 'result' : "init"});
      let currentProviderTask = null;
      if (isNonEmptyArray(customerTaskList) && isDefinedNotNull(currentProvider)) {
        currentProviderTask = customerTaskList.find((task) => task.targetUUID === currentProvider.uuid);
        if (isDefinedNotNull(currentProviderTask) && currentProviderTask.status !== "Success") {
          getCurrentTaskData(currentProviderTask.id);
          this.setState({currentTaskUUID: currentProviderTask.id, currentView: 'bootstrap'});
        }
      }
    }
  }

  componentWillReceiveProps(nextProps) {
    const {configuredProviders, cloud: {bootstrapProvider},
            cloudBootstrap: {data: { type }, promiseState},
            tasks: {customerTaskList}, providerType} = nextProps;
    const { refreshing } = this.state;
    if (refreshing && type === "initialize" && !promiseState.isLoading()) {
      this.setState({refreshing: false});
    }
    let currentProvider = null;
    if (configuredProviders.data) {
      currentProvider = configuredProviders.data.find((provider) => provider.code === providerType);
    }
    let currentProviderTask = null;
    if (!_.isEqual(configuredProviders.data, this.props.configuredProviders.data)) {
      this.setState({currentView: isNonEmptyObject(currentProvider) ? 'result' : 'init'});
    }

    if (getPromiseState(configuredProviders).isEmpty() && !getPromiseState(this.props.configuredProviders).isEmpty()) {
      this.setState({currentView: 'init'});
    }

    if (isNonEmptyArray(customerTaskList) && isNonEmptyObject(currentProvider) && this.props.tasks.customerTaskList.length === 0) {
      currentProviderTask = customerTaskList.find((task) => task.targetUUID === currentProvider.uuid);
      if (currentProviderTask) {
        this.props.getCurrentTaskData(currentProviderTask.id);
        if (isDefinedNotNull(currentProviderTask) && currentProviderTask.status !== "Success") {
          this.setState({currentTaskUUID: currentProviderTask.id, currentView: 'bootstrap'});
        }
      }
    }

    // If Provider Bootstrap task has started, go to provider bootstrap view.
    if (getPromiseState(this.props.cloud.bootstrapProvider).isLoading() && getPromiseState(bootstrapProvider).isSuccess()) {
      this.setState({currentTaskUUID: bootstrapProvider.data.taskUUID, currentView: 'bootstrap'});
      this.props.getCurrentTaskData(bootstrapProvider.data.taskUUID);
    }

    if (type === "initialize" && nextProps.cloudBootstrap.promiseState.name === "SUCCESS"
      && this.props.cloudBootstrap.promiseState.name === "LOADING") {
      this.setState({refreshSucceeded: true});
    }
  }


  getResultView() {
    const { configuredProviders, visibleModal, configuredRegions, universeList,
            accessKeys, hideDeleteProviderModal, initializeProvider, showDeleteProviderModal,
            deleteProviderConfig, providerType, hostInfo } = this.props;
    const currentProvider = configuredProviders.data.find((provider) => provider.code === providerType);
    let keyPairName = "Not Configured";
    if (isDefinedNotNull(accessKeys) && isNonEmptyArray(accessKeys.data)) {
      const currentAccessKey = accessKeys.data.find((accessKey) => accessKey.idKey.providerUUID === currentProvider.uuid);
      if (isDefinedNotNull(currentAccessKey)) {
        keyPairName = currentAccessKey.idKey.keyCode;
      }
    }
    let regions = [];
    if (isNonEmptyObject(currentProvider)) {
      if (isNonEmptyArray(configuredRegions.data)) {
        regions = configuredRegions.data.filter((region) => region.provider.uuid === currentProvider.uuid);
      }
      const providerInfo = [
        {name: "Name", data: currentProvider.name},
        {name: "SSH Key", data: keyPairName},
      ];
      if (currentProvider.code === "aws" && isNonEmptyObject(hostInfo)) {
        if (isNonEmptyString(hostInfo["region"])) {
          providerInfo.push({name: "Host Region", data: hostInfo["region"]});
        }
        if (isNonEmptyString(hostInfo["vpc-id"])) {
          providerInfo.push({name: "Host VPC ID", data: hostInfo["vpc-id"]});
        }
        if (isNonEmptyString(hostInfo["privateIp"])) {
          providerInfo.push({name: "Host Private IP", data: hostInfo["privateIp"]});
        }
      }
      let universeExistsForProvider = false;
      if (getPromiseState(configuredProviders).isSuccess() && getPromiseState(universeList).isSuccess()){
        universeExistsForProvider = universeList.data.some(universe => universe.provider && (universe.provider.uuid === currentProvider.uuid));
      }

      let currentModal = "";
      if (providerType === "aws") {
        currentModal = "deleteAWSProvider";
      } else if (providerType === "gcp") {
        currentModal = "deleteGCPProvider";
      }
      const deleteButtonDisabled = universeExistsForProvider;
      return (<ProviderResultView regions={regions} providerInfo={providerInfo}
                                 currentProvider={currentProvider}
                                 initializeMetadata={initializeProvider}
                                 showDeleteProviderModal={showDeleteProviderModal}
                                 visibleModal={visibleModal} deleteProviderConfig={deleteProviderConfig}
                                 hideDeleteProviderModal={hideDeleteProviderModal}
                                 currentModal={currentModal} providerType={providerType}
                                 deleteButtonDisabled={deleteButtonDisabled} refreshSucceeded={this.state.refreshSucceeded}/>);
    }
  }

  getBootstrapView() {
    const {configuredProviders, reloadCloudMetadata, cloud: {createProvider}, providerType, showDeleteProviderModal,
           visibleModal, deleteProviderConfig, hideDeleteProviderModal} = this.props;
    let currentModal = "";
    if (providerType === "aws") {
      currentModal = "deleteAWSProvider";
    } else if (providerType === "gcp") {
      currentModal = "deleteGCPProvider";
    }

    const currentConfiguredProvider = configuredProviders.data.find((provider) => provider.code === providerType);
    let provider = {};
    if (isNonEmptyObject(createProvider.data)) {
      provider = createProvider.data;
    } else if (isNonEmptyObject(currentConfiguredProvider)) {
      provider = currentConfiguredProvider;
    }

    return (<ProviderBootstrapView taskUUIDs={[this.state.currentTaskUUID]}
                                  currentProvider={provider}
                                  showDeleteProviderModal={showDeleteProviderModal}
                                  visibleModal={visibleModal}
                                  reloadCloudMetadata={reloadCloudMetadata}
                                  providerType={providerType}
                                  currentModal={currentModal}
                                  deleteProviderConfig={deleteProviderConfig}
                                  hideDeleteProviderModal={hideDeleteProviderModal}/>);
  }

  render() {
    let currentProviderView = <span/>;
    if (this.state.currentView === 'init') {
      currentProviderView = this.getInitView();
    } else if (this.state.currentView === "loading") {
      currentProviderView = <YBLoading />;
    } else if (this.state.currentView === 'bootstrap') {
      currentProviderView = this.getBootstrapView();
    } else if (this.state.currentView === 'result') {
      currentProviderView = this.getResultView();
    }
    return (
      <div className="provider-config-container">
        { currentProviderView }
      </div>
    );
  }
}

export default withRouter(ProviderConfiguration);
