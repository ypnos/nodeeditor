#include "DataFlowModel.hpp"

#include "Node.hpp"
#include "Connection.hpp"

namespace QtNodes {

DataFlowModel::DataFlowModel(std::shared_ptr<DataModelRegistry> registry) 
: _registry(std::move(registry)) {
}


// FlowSceneModel read interface
QStringList DataFlowModel::modelRegistry() const {
  QStringList list;
  for (const auto& item : _registry->registeredModels()) {
    list << item.first;
  }
  return list;
  }
  QString DataFlowModel::nodeTypeCategory(QString const& name) const {
  auto iter = _registry->registeredModelsCategoryAssociation().find(name);

  if (iter != _registry->registeredModelsCategoryAssociation().end()) {
    return iter->second;
  }
  return {};
}
QString DataFlowModel::converterNode(NodeDataType const& lhs, NodeDataType const& rhs) const {
  auto conv =  _registry->getTypeConverter(lhs.id, rhs.id);

  if (!conv) return {};

  return conv->name();
  }
  QList<QUuid> DataFlowModel::nodeUUids() const {
  QList<QUuid> ret;

  // extract the keys
  std::transform(_nodes.begin(), _nodes.end(), std::back_inserter(ret), [](const auto& pair) {
    return pair.first;
  });

  return ret;
}
NodeIndex DataFlowModel::nodeIndex(const QUuid& ID) const {
  auto iter = _nodes.find(ID);
  if (iter == _nodes.end()) return {};

  return createIndex(ID, iter->second.get());
  }
  QString DataFlowModel::nodeTypeIdentifier(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->name();
}
QString DataFlowModel::nodeCaption(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  if (!node->nodeDataModel()->captionVisible()) return {};

  return node->nodeDataModel()->caption();
  }
  QPointF DataFlowModel::nodeLocation(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->position();
}
QWidget* DataFlowModel::nodeWidget(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->embeddedWidget();
}
bool DataFlowModel::nodeResizable(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->resizable();
}
NodeValidationState DataFlowModel::nodeValidationState(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->validationState();
  }
  QString DataFlowModel::nodeValidationMessage(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->validationMessage();
}
NodePainterDelegate* DataFlowModel::nodePainterDelegate(NodeIndex const& index) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->painterDelegate();
  }
  unsigned int DataFlowModel::nodePortCount(NodeIndex const& index, PortType portType) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->nPorts(portType);
}
QString DataFlowModel::nodePortCaption(NodeIndex const& index, PortType portType, PortIndex pIndex) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->portCaption(portType, pIndex);
}
NodeDataType DataFlowModel::nodePortDataType(NodeIndex const& index, PortType portType, PortIndex pIndex) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  return node->nodeDataModel()->dataType(portType, pIndex);
}
ConnectionPolicy DataFlowModel::nodePortConnectionPolicy(NodeIndex const& index, PortType portType, PortIndex pIndex) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  if (portType == PortType::In) {
    return ConnectionPolicy::One;
  }
  return node->nodeDataModel()->portOutConnectionPolicy(pIndex);
}
std::vector<std::pair<NodeIndex, PortIndex>> DataFlowModel::nodePortConnections(NodeIndex const& index, PortType portType, PortIndex id) const {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  std::vector<std::pair<NodeIndex, PortIndex>> ret;
  // construct connections
  for (const auto& conn : node->connections(portType, id)) {
    ret.emplace_back(nodeIndex(conn->getNode(oppositePort(portType))->id()), conn->getPortIndex(oppositePort(portType)));
  }
  return ret;
}

// FlowSceneModel write interface
bool DataFlowModel::removeConnection(NodeIndex const& leftNodeIdx, PortIndex leftPortID, NodeIndex const& rightNodeIdx, PortIndex rightPortID) {
  Q_ASSERT(leftNodeIdx.isValid());
  Q_ASSERT(rightNodeIdx.isValid());

  auto* leftNode = static_cast<Node*>(leftNodeIdx.internalPointer());
  auto* rightNode = static_cast<Node*>(rightNodeIdx.internalPointer());

  ConnectionID connID;
  connID.lNodeID = leftNodeIdx.id();
  connID.rNodeID = rightNodeIdx.id();
  connID.lPortID = leftPortID;
  connID.rPortID = rightPortID;

  // update the node
  _connections[connID]->propagateEmptyData();

  // remove it from the nodes
  auto& leftConns = leftNode->connections(PortType::Out, leftPortID);
  auto iter = std::find_if(leftConns.begin(), leftConns.end(), [&](Connection* conn){ return conn->id() == connID; });
  Q_ASSERT(iter != leftConns.end());
  leftConns.erase(iter);

  auto& rightConns = rightNode->connections(PortType::In, rightPortID);
  iter = std::find_if(rightConns.begin(), rightConns.end(), [&](Connection* conn){ return conn->id() == connID; });
  Q_ASSERT(iter != rightConns.end());
  rightConns.erase(iter);

  emit connectionAboutToBeRemoved(leftNodeIdx, leftPortID, rightNodeIdx, rightPortID);

  // remove it from the map
  _connections.erase(connID);

  // tell the view
  emit connectionRemoved(leftNodeIdx, leftPortID, rightNodeIdx, rightPortID);

  return true;
}
bool DataFlowModel::addConnection(NodeIndex const& leftNodeIdx, PortIndex leftPortID, NodeIndex const& rightNodeIdx, PortIndex rightPortID) {
  Q_ASSERT(leftNodeIdx.isValid());
  Q_ASSERT(rightNodeIdx.isValid());

  auto* leftNode = static_cast<Node*>(leftNodeIdx.internalPointer());
  auto* rightNode = static_cast<Node*>(rightNodeIdx.internalPointer());

  ConnectionID connID;
  connID.lNodeID = leftNodeIdx.id();
  connID.rNodeID = rightNodeIdx.id();
  connID.lPortID = leftPortID;
  connID.rPortID = rightPortID;

  // create the connection
  auto conn = std::make_shared<Connection>(*rightNode, rightPortID, *leftNode, leftPortID);
  _connections[connID] = conn;

  // add it to the nodes
  leftNode->connections(PortType::Out, leftPortID).push_back(conn.get());
  rightNode->connections(PortType::In, rightPortID).push_back(conn.get());

  // update the node
  _connections[connID]->propagateData(leftNode->nodeDataModel()->outData(leftPortID));

  // tell the view the connection was added
  emit connectionAdded(leftNodeIdx, leftPortID, rightNodeIdx, rightPortID);

  return true;
}
bool DataFlowModel::removeNode(NodeIndex const& index) {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());

  // make sure there are no connections left
  #ifndef NDEBUG
  for (PortIndex idx = 0; (unsigned)idx < node->nodeDataModel()->nPorts(PortType::In); ++idx) {
    Q_ASSERT(node->connections(PortType::In, idx).empty());
  }
  for (PortIndex idx = 0; (unsigned)idx < node->nodeDataModel()->nPorts(PortType::Out); ++idx) {
    Q_ASSERT(node->connections(PortType::Out, idx).empty());
  }
  #endif

  emit nodeAboutToBeRemoved(index);

  // remove it from the map
  _nodes.erase(index.id());

  // tell the view
  emit nodeRemoved(index.id());

  return true;
}
QUuid DataFlowModel::addNode(const QString& typeID, QPointF const&) {
  // create the NodeDataModel
  auto model = _registry->create(typeID);
  if (!model) {
    return {};
  }

  return addNode(std::move(model)).id();
}

Node&
DataFlowModel::
addNode(std::unique_ptr<NodeDataModel>&& model) {
  // create the UUID
  QUuid nodeid = QUuid::createUuid();
  
  // create a node
  auto modelPtr = model.get(); // cache the ptr
  auto node = std::make_unique<Node>(std::move(model), nodeid);

  // cache the pointer so the connection can be made
  auto nodePtr = node.get();

  // add it to the map
  _nodes[nodeid] = std::move(node);

  // connect to the geometry gets updated
  connect(nodePtr, &Node::positionChanged, this, [this, nodeid](QPointF const&){ nodeMoved(nodeIndex(nodeid)); });

  // connect to data changes
  connect(modelPtr, &NodeDataModel::dataUpdated, this, [nodePtr](PortIndex id) {
    nodePtr->onDataUpdated(id);
    for (const auto& conn : nodePtr->connections(PortType::Out, id)) {
      conn->propagateData(nodePtr->nodeDataModel()->outData(id));
    }
  });

  // tell the view
  emit nodeAdded(nodeid);
  
  return *nodePtr;
}

bool DataFlowModel::moveNode(NodeIndex const& index, QPointF newLocation) {
  Q_ASSERT(index.isValid());

  auto* node = static_cast<Node*>(index.internalPointer());
  node->setPosition(newLocation);

  // no need to emit, it's done by the function already
  return true;
}

void DataFlowModel::nodeDoubleClicked(NodeIndex const& index, QPoint const&) {
  emit nodeDoubleClickedSignal(*_nodes[index.id()]);
}

void DataFlowModel::connectionHovered(NodeIndex const& lhs, PortIndex lPortIndex, NodeIndex const& rhs, PortIndex rPortIndex, QPoint const& pos, bool entered) {
  ConnectionID id;
  id.lNodeID = lhs.id();
  id.rNodeID = rhs.id();
  id.lPortID = lPortIndex;
  id.rPortID = rPortIndex;

  if (entered) {
    emit connectionHoveredEnteredSignal(*_connections[id], pos);
  } else {
    emit connectionHoveredLeftSignal(*_connections[id], pos);
  }
}
void DataFlowModel::nodeHovered(NodeIndex const& index, QPoint const& pos, bool entered) {
  if (entered) {
    emit nodeHoveredEnteredSignal(*_nodes[index.id()], pos);
  } else {
    emit nodeHoveredLeftSignal(*_nodes[index.id()], pos);
  }
}

} // namespace QtNodes
