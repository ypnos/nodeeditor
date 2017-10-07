#pragma once

#include "FlowScene.hpp"
#include "PortType.hpp"
#include "FlowSceneModel.hpp"
#include "QUuidStdHash.hpp"
#include "Node.hpp"
#include "ConnectionID.hpp"
#include "Export.hpp"
#include "DataFlowModel.hpp"

#include <memory>
#include <functional>
#include <unordered_map>

namespace QtNodes {
  
class Connection;
class DataModelRegistry;
class NodeDataModel;

class NODE_EDITOR_PUBLIC DataFlowScene : public FlowScene {
  Q_OBJECT

public:

  DataFlowScene(std::shared_ptr<DataModelRegistry> registry =
              std::make_shared<DataModelRegistry>());

  std::shared_ptr<Connection>createConnection(Node& nodeIn,
                                              PortIndex portIndexIn,
                                              Node& nodeOut,
                                              PortIndex portIndexOut);

  std::shared_ptr<Connection>restoreConnection(QJsonObject const &connectionJson);

  void deleteConnection(Connection& connection);

  Node& createNode(std::unique_ptr<NodeDataModel> && dataModel);

  Node& restoreNode(QJsonObject const& nodeJson);

  void removeNode(Node& node);

  DataModelRegistry& registry() const;

  void setRegistry(std::shared_ptr<DataModelRegistry> registry);

  void iterateOverNodes(std::function<void(Node*)> visitor);

  void iterateOverNodeData(std::function<void(NodeDataModel*)> visitor);

  void iterateOverNodeDataDependentOrder(std::function<void(NodeDataModel*)> visitor);

  QPointF getNodePosition(const Node& node) const;

  void setNodePosition(Node& node, const QPointF& pos) const;

  QSizeF getNodeSize(const Node& node) const;
public:

  std::unordered_map<QUuid, std::unique_ptr<Node> > const &nodes() const;

  std::unordered_map<ConnectionID, std::shared_ptr<Connection> > const &connections() const;

  std::vector<Node*>selectedNodes() const;

public:

  void clearScene();

  void save() const;

  void load();

  QByteArray saveToMemory() const;

  void loadFromMemory(const QByteArray& data);

signals:

  void nodeCreated(Node &n);

  void nodeDeleted(Node &n);

  void connectionCreated(Connection &c);
  void connectionDeleted(Connection &c);

  void nodeMoved(Node& n, const QPointF& newLocation);

  void nodeDoubleClicked(Node& n);

  void connectionHovered(Connection& c, QPoint screenPos);

  void nodeHovered(Node& n, QPoint screenPos);

  void connectionHoverLeft(Connection& c);

  void nodeHoverLeft(Node& n);
  
private:
  
  DataFlowModel* _dataFlowModel;

};

} // namespace QtNodes
