#include "taco/expr/expr.h"

#include "error/error_checks.h"
#include "error/error_messages.h"
#include "taco/type.h"
#include "taco/format.h"
#include "taco/expr/schedule.h"
#include "taco/expr/expr_nodes.h"
#include "taco/util/name_generator.h"

using namespace std;

namespace taco {

// class IndexVar
struct IndexVar::Content {
  string name;
};

IndexVar::IndexVar() : IndexVar(util::uniqueName('i')) {}

IndexVar::IndexVar(const std::string& name) : content(new Content) {
  content->name = name;
}

std::string IndexVar::getName() const {
  return content->name;
}

bool operator==(const IndexVar& a, const IndexVar& b) {
  return a.content == b.content;
}

bool operator<(const IndexVar& a, const IndexVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const IndexVar& var) {
  return os << var.getName();
}


// class TensorVar
struct TensorVar::Content {
  string name;
  Type type;
  Format format;

  vector<IndexVar> freeVars;
  IndexExpr indexExpr;
  bool accumulate;

  Schedule schedule;
};

TensorVar::TensorVar() : TensorVar(Type()) {
}

TensorVar::TensorVar(const Type& type) : TensorVar(type, Dense) {
}

TensorVar::TensorVar(const std::string& name, const Type& type)
    : TensorVar(name, type, Dense) {
}

TensorVar::TensorVar(const Type& type, const Format& format)
    : TensorVar(util::uniqueName('A'), type, format) {
}

TensorVar::TensorVar(const string& name, const Type& type, const Format& format)
    : content(new Content) {
  content->name = name;
  content->type = type;
  content->format = format;
}

std::string TensorVar::getName() const {
  return content->name;
}

const Type& TensorVar::getType() const {
  return content->type;
}

const Format& TensorVar::getFormat() const {
  return content->format;
}

const std::vector<IndexVar>& TensorVar::getFreeVars() const {
  return content->freeVars;
}

const IndexExpr& TensorVar::getIndexExpr() const {
  return content->indexExpr;
}

bool TensorVar::isAccumulating() const {
  return content->accumulate;
}

const Schedule& TensorVar::getSchedule() const {
  struct GetSchedule : public ExprVisitor {
    Schedule schedule;
    void visit(const BinaryExprNode* expr) {
      for (auto& operatorSplit : expr->getOperatorSplits()) {
        schedule.addOperatorSplit(operatorSplit);
      }
    }
  };
  GetSchedule getSchedule;
  content->schedule.clearOperatorSplits();
  getSchedule.schedule = content->schedule;
  getIndexExpr().accept(&getSchedule);
  return content->schedule;
}


void TensorVar::setIndexExpression(vector<IndexVar> freeVars,
                                   IndexExpr indexExpr, bool accumulate) {
  auto shape = getType().getShape();
  taco_uassert(error::dimensionsTypecheck(freeVars, indexExpr, shape))
      << error::expr_dimension_mismatch << " "
      << error::dimensionTypecheckErrors(freeVars, indexExpr, shape);

  // The following are index expressions the implementation doesn't currently
  // support, but that are planned for the future.
  taco_uassert(!error::containsTranspose(this->getFormat(), freeVars, indexExpr))
      << error::expr_transposition;
  taco_uassert(!error::containsDistribution(freeVars, indexExpr))
      << error::expr_distribution;

  content->freeVars = freeVars;
  content->indexExpr = indexExpr;
  content->accumulate = accumulate;
}

bool operator==(const TensorVar& a, const TensorVar& b) {
  return a.content == b.content;
}

bool operator<(const TensorVar& a, const TensorVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const TensorVar& var) {
  return os << var.getName() << " : " << var.getType();
}

set<IndexVar> getIndexVars(const TensorVar& tensor) {
  set<IndexVar> indexVars(tensor.getFreeVars().begin(), tensor.getFreeVars().end());
  match(tensor.getIndexExpr(),
    function<void(const AccessNode*)>([&indexVars](const AccessNode* op) {
      indexVars.insert(op->indexVars.begin(), op->indexVars.end());
    })
  );
  return indexVars;
}

map<IndexVar,Dimension> getIndexVarRanges(const TensorVar& tensor) {
  map<IndexVar, Dimension> indexVarRanges;

  auto& freeVars = tensor.getFreeVars();
  auto& type = tensor.getType();
  for (size_t i = 0; i < freeVars.size(); i++) {
    indexVarRanges.insert({freeVars[i], type.getShape().getDimension(i)});
  }

  match(tensor.getIndexExpr(),
    function<void(const AccessNode*)>([&indexVarRanges](const AccessNode* op) {
      auto& tensor = op->tensorVar;
      auto& vars = op->indexVars;
      auto& type = tensor.getType();
      for (size_t i = 0; i < vars.size(); i++) {
        indexVarRanges.insert({vars[i], type.getShape().getDimension(i)});
      }
    })
  );
  
  return indexVarRanges;
}


// class ExprNode
ExprNode::ExprNode() : operatorSplits(new vector<OperatorSplit>) {
  }

void ExprNode::splitOperator(IndexVar old, IndexVar left, IndexVar right) {
  operatorSplits->push_back(OperatorSplit(this, old, left, right));
}

const std::vector<OperatorSplit>& ExprNode::getOperatorSplits() const {
  return *operatorSplits;
}


// class IndexExpr
IndexExpr::IndexExpr(int val) : IndexExpr(new IntImmNode(val)) {
}

IndexExpr::IndexExpr(double val) : IndexExpr(new DoubleImmNode(val)) {
}

IndexExpr::IndexExpr(float val) : IndexExpr(new FloatImmNode(val)) {
}

IndexExpr IndexExpr::operator-() {
  return new NegNode(this->ptr);
}

void IndexExpr::splitOperator(IndexVar old, IndexVar left, IndexVar right) {
  const_cast<ExprNode*>(this->ptr)->splitOperator(old, left, right);
}

void IndexExpr::accept(ExprVisitorStrict *v) const {
  ptr->accept(v);
}

std::ostream& operator<<(std::ostream& os, const IndexExpr& expr) {
  if (!expr.defined()) return os << "Expr()";
  expr.ptr->print(os);
  return os;
}


// class Read
Access::Access(const Node* n) : IndexExpr(n) {
}

Access::Access(const TensorVar& tensor, const std::vector<IndexVar>& indices)
    : Access(new Node(tensor, indices)) {
}

const Access::Node* Access::getPtr() const {
  return static_cast<const Node*>(ptr);
}

const TensorVar& Access::getTensorVar() const {
  return getPtr()->tensorVar;
}

const std::vector<IndexVar>& Access::getIndexVars() const {
  return getPtr()->indexVars;
}

void Access::operator=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  taco_uassert(!result.getIndexExpr().defined()) << "Cannot reassign " <<result;
  result.setIndexExpression(getIndexVars(), expr);
}

void Access::operator=(const Access& expr) {
  operator=(static_cast<IndexExpr>(expr));
}

void Access::operator+=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  taco_uassert(!result.getIndexExpr().defined()) << "Cannot reassign " <<result;
  // TODO: check that result format is dense. For now only support accumulation
  /// into dense. If it's not dense, then we can insert an operator split.
  result.setIndexExpression(getIndexVars(), expr, true);
}

void Access::operator+=(const Access& expr) {
  operator+=(static_cast<IndexExpr>(expr));
}

// Operators
IndexExpr operator+(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new AddNode(lhs, rhs);
}

IndexExpr operator-(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new SubNode(lhs, rhs);
}

IndexExpr operator*(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new MulNode(lhs, rhs);
}

IndexExpr operator/(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new DivNode(lhs, rhs);
}

}
