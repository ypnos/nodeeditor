#pragma once

#include "FlowSceneModel.hpp"
#include "DataModelRegistry.hpp"
#include "ConnectionID.hpp"
#include "Node.hpp"
#include "Connection.hpp"
#include "QUuidStdHash.hpp"

#include <unordered_map>
#include <memory>

#include <QUuid>

namespace QtNodes {

// default model class
class DataFlowModel : public FlowSceneModel {
  Q_OBJECT
public:

  DataFlowModel(std::shared_ptr<DataModelRegistry> reg);

  // FlowSceneModel read interface
  QStringList modelRegistry() const override;
  QString nodeTypeCategory(QString const& /*name*/) const override;
  QString converterNode(NodeDataType const& /*lhs*/, NodeDataType const& ) const override;
  QList<QUuid> nodeUUids() const override;
  NodeIndex nodeIndex(const QUuid& ID) const override;
  QString nodeTypeIdentifier(NodeIndex const& index) const override;
  QString nodeCaption(NodeIndex const& index) const override;
  QPointF nodeLocation(NodeIndex const& index) const override;
  QWidget* nodeWidget(NodeIndex const& index) const override;
  bool nodeResizable(NodeIndex const& index) const override;
  NodeValidationState nodeValidationState(NodeIndex const& index) const override;
  QString nodeValidationMessage(NodeIndex const& index) const override;
  NodePainterDelegate* nodePainterDelegate(NodeIndex const& index) const override;
  unsigned int nodePortCount(NodeIndex const& index, PortType portType) const override;
  QString nodePortCaption(NodeIndex const& index, PortType portType, PortIndex pIndex) const override;
  NodeDataType nodePortDataType(NodeIndex const& index, PortType portType, PortIndex pIndex) const override;
  ConnectionPolicy nodePortConnectionPolicy(NodeIndex const& index, PortType portType, PortIndex pIndex) const override;
  std::vector<std::pair<NodeIndex, PortIndex>> nodePortConnections(NodeIndex const& index, PortType portType, PortIndex id) const override;

  // FlowSceneModel write interface
  bool removeConnection(NodeIndex const& leftNode, PortIndex leftPortID, NodeIndex const& rightNode, PortIndex rightPortID) override;
  bool addConnection(NodeIndex const& leftNode, PortIndex leftPortID, NodeIndex const& rightNode, PortIndex rightPortID) override;
  bool removeNode(NodeIndex const& index) override;
  QUuid addNode(const QString& typeID, QPointF const& location) override;
  Node& addNode(std::unique_ptr<NodeDataModel>&& model);
  bool moveNode(NodeIndex const& index, QPointF newLocation) override;

  // notifications
  void nodeDoubleClicked(NodeIndex const& index, QPoint const& pos) override;
  void connectionHovered(NodeIndex const& lhs, PortIndex lPortIndex, NodeIndex const& rhs, PortIndex rPortIndex, QPoint const& pos, bool entered) override;
  void nodeHovered(NodeIndex const& index, QPoint const& pos, bool entered) override;

signals:

  void nodeDoubleClickedSignal(Node& node);
  void connectionHoveredEnteredSignal(Connection& c, QPoint screenPos);
  void connectionHoveredLeftSignal(Connection& c, QPoint screenPos);
  void nodeHoveredEnteredSignal(Node& n, QPoint screenPos);
  void nodeHoveredLeftSignal(Node& n, QPoint screenPos);

public:

  using SharedConnection = std::shared_ptr<Connection>;
  using UniqueNode       = std::unique_ptr<Node>;

  std::unordered_map<ConnectionID, SharedConnection> _connections;
  std::unordered_map<QUuid, UniqueNode>              _nodes;
  std::shared_ptr<DataModelRegistry>                 _registry;

};
} // namespace QtNodes