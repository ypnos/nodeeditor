#include "DataFlowScene.hpp"
#include "Connection.hpp"
#include "DataFlowModel.hpp"

#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>

namespace QtNodes {

DataFlowScene::DataFlowScene(std::shared_ptr<DataModelRegistry> registry) : FlowScene(new DataFlowModel(std::move(registry))) {
  _dataFlowModel = static_cast<DataFlowModel*>(model());

  // connect up the signals
  connect(_dataFlowModel, &FlowSceneModel::nodeAdded, this, [this](const QUuid& uuid) {
    emit nodeCreated(*_dataFlowModel->_nodes[uuid]);
  });
  connect(_dataFlowModel, &FlowSceneModel::nodeAboutToBeRemoved, this, [this](NodeIndex const& index) {
    emit nodeDeleted(*_dataFlowModel->_nodes[index.id()]);
  });
  connect(_dataFlowModel, &FlowSceneModel::connectionAdded, this, [this](NodeIndex const& leftNode, PortIndex leftPortID, NodeIndex const& rightNode, PortIndex rightPortID) {
    ConnectionID id;
    id.lNodeID = leftNode.id();
    id.rNodeID = rightNode.id();
    id.lPortID = leftPortID;
    id.rPortID = rightPortID;
    emit connectionCreated(*_dataFlowModel->_connections[id]);
  });
  connect(_dataFlowModel, &FlowSceneModel::connectionAboutToBeRemoved, this, [this](NodeIndex const& leftNode, PortIndex leftPortID, NodeIndex const& rightNode, PortIndex rightPortID) {
    ConnectionID id;
    id.lNodeID = leftNode.id();
    id.rNodeID = rightNode.id();
    id.lPortID = leftPortID;
    id.rPortID = rightPortID;
    emit connectionDeleted(*_dataFlowModel->_connections[id]);
  });
  connect(_dataFlowModel, &FlowSceneModel::nodeMoved, this, [this](NodeIndex const& index) {
    emit nodeMoved(*_dataFlowModel->_nodes[index.id()], _dataFlowModel->nodeLocation(index));
  });
  connect(_dataFlowModel, &DataFlowModel::nodeDoubleClickedSignal, this, &DataFlowScene::nodeDoubleClicked);

  connect(_dataFlowModel, &DataFlowModel::nodeHoveredEnteredSignal, this, &DataFlowScene::nodeHovered);
  connect(_dataFlowModel, &DataFlowModel::nodeHoveredLeftSignal, this, &DataFlowScene::nodeHoverLeft);
  connect(_dataFlowModel, &DataFlowModel::connectionHoveredEnteredSignal, this, &DataFlowScene::connectionHovered);
  connect(_dataFlowModel, &DataFlowModel::connectionHoveredLeftSignal, this, &DataFlowScene::connectionHoverLeft);
  
}

std::shared_ptr<Connection>
DataFlowScene::
createConnection(Node& nodeIn,
  PortIndex portIndexIn, Node& nodeOut, PortIndex portIndexOut) 
{
  _dataFlowModel->addConnection(_dataFlowModel->nodeIndex(nodeOut.id()), portIndexOut, _dataFlowModel->nodeIndex(nodeIn.id()), portIndexIn);

  ConnectionID id;
  id.lNodeID = nodeOut.id();
  id.rNodeID = nodeIn.id();
  id.lPortID = portIndexOut;
  id.rPortID = portIndexIn;

  return _dataFlowModel->_connections[id];
}

std::shared_ptr<Connection>
DataFlowScene::
restoreConnection(QJsonObject const &connectionJson)
{
  QUuid nodeInId  = QUuid(connectionJson["in_id"].toString());
  QUuid nodeOutId = QUuid(connectionJson["out_id"].toString());

  PortIndex portIndexIn  = connectionJson["in_index"].toInt();
  PortIndex portIndexOut = connectionJson["out_index"].toInt();


  ConnectionID connId;
  connId.lNodeID = nodeOutId;
  connId.rNodeID = nodeInId;
  
  connId.lPortID = portIndexOut;
  connId.rPortID = portIndexIn;
  
  if (!_dataFlowModel->addConnection(_dataFlowModel->nodeIndex(nodeOutId), connId.lPortID, _dataFlowModel->nodeIndex(nodeInId), connId.rPortID)) 
    return nullptr;

  return _dataFlowModel->_connections[connId];
  
}

void
DataFlowScene::
deleteConnection(Connection& connection) {
  _dataFlowModel->removeConnection(_dataFlowModel->nodeIndex(connection.getNode(PortType::Out)->id()), connection.getPortIndex(PortType::Out), 
    _dataFlowModel->nodeIndex(connection.getNode(PortType::In)->id()), connection.getPortIndex(PortType::In));
}

Node&
DataFlowScene::
createNode(std::unique_ptr<NodeDataModel>&& dataModel) {
  return _dataFlowModel->addNode(std::move(dataModel));
}

Node&
DataFlowScene::
restoreNode(QJsonObject const& nodeJson)
{
  QString modelName = nodeJson["model"].toObject()["name"].toString();

  auto uuid = _dataFlowModel->addNode(modelName, {});
  
  if (uuid.isNull())
    throw std::logic_error(std::string("No registered model with name ") +
                           modelName.toLocal8Bit().data());

  auto& node = *_dataFlowModel->_nodes[uuid];
  node.restore(nodeJson);

  return node;
}

void 
DataFlowScene::
removeNode(Node& node) {
  model()->removeNodeWithConnections(model()->nodeIndex(node.id()));
}

DataModelRegistry& 
DataFlowScene::
registry() const {
  return *_dataFlowModel->_registry;
}


void
DataFlowScene::
setRegistry(std::shared_ptr<DataModelRegistry> registry) {
  _dataFlowModel->_registry = std::move(registry);
}

void
DataFlowScene::
iterateOverNodes(std::function<void(Node*)> visitor) {
  for (const auto& node : _dataFlowModel->_nodes) {
    visitor(node.second.get());
  }
}

void
DataFlowScene::
iterateOverNodeData(std::function<void(NodeDataModel*)> visitor) {
  for (const auto& node : _dataFlowModel->_nodes) {
    visitor(node.second->nodeDataModel());
  }
}

QPointF
DataFlowScene::
getNodePosition(const Node& node) const {
  return model()->nodeLocation(model()->nodeIndex(node.id()));
}

void
DataFlowScene::
setNodePosition(Node& node, const QPointF& pos) const {
  model()->moveNode(model()->nodeIndex(node.id()), pos);
}

void
DataFlowScene::
iterateOverNodeDataDependentOrder(std::function<void(NodeDataModel*)> visitor) {
  std::set<QUuid> visitedNodesSet;

  //A leaf node is a node with no input ports, or all possible input ports empty
  auto isNodeLeaf =
    [](NodeGraphicsObject const &node, NodeDataModel const &model)
    {
      for (size_t i = 0; i < model.nPorts(PortType::In); ++i)
      {
        auto connections = node.nodeState().connections(PortType::In, i);
        if (!connections.empty())
        {
          return false;
        }
      }

      return true;
    };

  //Iterate over "leaf" nodes
  for (auto const &_node : _dataFlowModel->_nodes)
  {
    auto const &node = *_node.second;
    auto model       = node.nodeDataModel();

    if (isNodeLeaf(*nodeGraphicsObject(node.id()), *model))
    {
      visitor(model);
      visitedNodesSet.insert(node.id());
    }
  }

  auto areNodeInputsVisitedBefore =
    [&](NodeGraphicsObject const &node, NodeDataModel const &model)
    {
      for (size_t i = 0; i < model.nPorts(PortType::In); ++i)
      {
        auto connections = node.nodeState().connections(PortType::In, i);

        for (auto& conn : connections)
        {
          if (visitedNodesSet.find(conn->node(PortType::Out).id()) == visitedNodesSet.end())
          {
            return false;
          }
        }
      }

      return true;
    };

  //Iterate over dependent nodes
  while (_dataFlowModel->_nodes.size() != visitedNodesSet.size())
  {
    for (auto const &_node : _dataFlowModel->_nodes)
    {
      auto const &node = _node.second;
      if (visitedNodesSet.find(node->id()) != visitedNodesSet.end())
        continue;

      auto model = node->nodeDataModel();

      if (areNodeInputsVisitedBefore(*nodeGraphicsObject(node->id()), *model))
      {
        visitor(model);
        visitedNodesSet.insert(node->id());
      }
    }
  }
}

QSizeF
DataFlowScene::
getNodeSize(const Node& node) const {
  auto ngo = nodeGraphicsObject(model()->nodeIndex(node.id()));

  return QSizeF(ngo->geometry().width(), ngo->geometry().height());
  
}

std::unordered_map<QUuid, std::unique_ptr<Node> > const &
DataFlowScene::
nodes() const {
  return _dataFlowModel->_nodes;
}

std::unordered_map<ConnectionID, std::shared_ptr<Connection> > const &
DataFlowScene::
connections() const {
  return _dataFlowModel->_connections;
}

std::vector<Node*>
DataFlowScene::
selectedNodes() const {
  QList<QGraphicsItem*> graphicsItems = selectedItems();
  
  std::vector<Node*> ret;
  ret.reserve(graphicsItems.size());

  for (QGraphicsItem* item : graphicsItems)
  {
    auto ngo = qgraphicsitem_cast<NodeGraphicsObject*>(item);

    if (ngo != nullptr)
    {
      ret.push_back(_dataFlowModel->_nodes[ngo->index().id()].get());
    }
  }

  return ret;
}


void
DataFlowScene::
clearScene() {
  // delete all the nodes
  while(!_dataFlowModel->_nodes.empty()) {
    removeNode(*_dataFlowModel->_nodes.begin()->second);
  }
}

void
DataFlowScene::
save() const
{
  QString fileName =
    QFileDialog::getSaveFileName(nullptr,
                                 tr("Open Flow Scene"),
                                 QDir::homePath(),
                                 tr("Flow Scene Files (*.flow)"));

  if (!fileName.isEmpty())
  {
    if (!fileName.endsWith("flow", Qt::CaseInsensitive))
      fileName += ".flow";

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
      file.write(saveToMemory());
    }
  }
}


void
DataFlowScene::
load()
{
  clearScene();

  //-------------

  QString fileName =
    QFileDialog::getOpenFileName(nullptr,
                                 tr("Open Flow Scene"),
                                 QDir::homePath(),
                                 tr("Flow Scene Files (*.flow)"));

  if (!QFileInfo::exists(fileName))
    return;

  QFile file(fileName);

  if (!file.open(QIODevice::ReadOnly))
    return;

  QByteArray wholeFile = file.readAll();

  loadFromMemory(wholeFile);
}


QByteArray
DataFlowScene::
saveToMemory() const
{
  QJsonObject sceneJson;

  QJsonArray nodesJsonArray;

  for (auto const & pair : _dataFlowModel->_nodes)
  {
    auto const &node = pair.second;

    nodesJsonArray.append(node->save());
  }

  sceneJson["nodes"] = nodesJsonArray;

  QJsonArray connectionJsonArray;
  for (auto const & pair : _dataFlowModel->_connections)
  {
    auto const &connection = pair.second;

    QJsonObject connectionJson = connection->save();

    if (!connectionJson.isEmpty())
      connectionJsonArray.append(connectionJson);
  }

  sceneJson["connections"] = connectionJsonArray;

  QJsonDocument document(sceneJson);

  return document.toJson();
}


void
DataFlowScene::
loadFromMemory(const QByteArray& data)
{
  QJsonObject const jsonDocument = QJsonDocument::fromJson(data).object();

  QJsonArray nodesJsonArray = jsonDocument["nodes"].toArray();

  for (int i = 0; i < nodesJsonArray.size(); ++i)
  {
    restoreNode(nodesJsonArray[i].toObject());
  }

  QJsonArray connectionJsonArray = jsonDocument["connections"].toArray();

  for (int i = 0; i < connectionJsonArray.size(); ++i)
  {
    restoreConnection(connectionJsonArray[i].toObject());
  }
}

} // namespace QtNodes
