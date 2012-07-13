/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include <Interface/Modules/EvaluateLinearAlgebraBinaryDialog.h>
#include <Algorithms/Math/EvaluateLinearAlgebraBinary.h>
#include <Interface/Application/Logger.h>
#include <Core/Dataflow/Network/ModuleStateInterface.h>  //TODO: extract into intermediate

using namespace SCIRun::Gui;
using namespace SCIRun::Domain::Networks;
using namespace SCIRun::Algorithms::Math;

EvaluateLinearAlgebraBinaryDialog::EvaluateLinearAlgebraBinaryDialog(const std::string& name, ModuleStateHandle state,
  QWidget* parent /* = 0 */)
  : ModuleDialogGeneric(parent),
  state_(state)
{
  setupUi(this);
  setWindowTitle(to_QString(name));
  executionTimeHorizontalSlider_->setValue(moduleExecutionTime());
  
  connect(executeButton_, SIGNAL(clicked()), this, SIGNAL(executeButtonPressed()));
  connect(addRadioButton_, SIGNAL(clicked()), this, SLOT(pushOperationToState()));
  connect(subtractRadioButton_, SIGNAL(clicked()), this, SLOT(pushOperationToState()));
  connect(multiplyRadioButton_, SIGNAL(clicked()), this, SLOT(pushOperationToState()));
}

int EvaluateLinearAlgebraBinaryDialog::moduleExecutionTime()
{
  return 2000;
}

int EvaluateLinearAlgebraBinaryDialog::getSelectedOperator() const
{
  if (addRadioButton_->isChecked())
    return (int)EvaluateLinearAlgebraBinaryAlgorithm::ADD;
  if (subtractRadioButton_->isChecked())
    return (int)EvaluateLinearAlgebraBinaryAlgorithm::SUBTRACT;
  if (multiplyRadioButton_->isChecked())
    return (int)EvaluateLinearAlgebraBinaryAlgorithm::MULTIPLY;
  else
    return -1;
}

void EvaluateLinearAlgebraBinaryDialog::pushOperationToState() 
{
  EvaluateLinearAlgebraBinaryAlgorithm::Operator op = (EvaluateLinearAlgebraBinaryAlgorithm::Operator) getSelectedOperator();

  state_->setValue("Operation", op);
}
