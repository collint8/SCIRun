/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
   University of Utah.

   
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

#include <Core/Algorithms/Legacy/Fields/FieldData/SwapFieldDataWithMatrixEntriesAlgo.h>
#include <Core/Datatypes/Legacy/Field/Field.h>
#include <Core/Datatypes/Matrix.h>
#include <Core/Datatypes/SparseRowMatrix.h>
#include <Core/Datatypes/Legacy/Field/FieldInformation.h>
#include <Dataflow/Network/ModuleStateInterface.h>
#include <Core/Datatypes/Legacy/Base/PropertyManager.h>
#include <Core/Datatypes/Legacy/Matrix/MatrixTypeConverter.h>
#include <Core/Algorithms/Base/AlgorithmVariableNames.h>
#include <Core/Algorithms/Base/AlgorithmPreconditions.h> 

using namespace SCIRun::Core::Algorithms;
using namespace SCIRun::Core::Algorithms::Fields;
using namespace SCIRun::Core::Algorithms::Fields::Parameters; 
using namespace SCIRun::Dataflow::Networks;
using namespace SCIRun::Core::Datatypes;
using namespace SCIRun;

ALGORITHM_PARAMETER_DEF(Fields, PreserveScalar)

SwapFieldDataWithMatrixEntries::SwapFieldDataWithMatrixEntries()
{
  addParameter(PreserveScalar, false);
}

bool
SwapFieldDataWithMatrixEntries::runImpl(FieldHandle input, FieldHandle& output, MatrixHandle& mapping) const
{
  ScopedAlgorithmStatusReporter r(this, "ConvertFieldBasis");

  if (!input)
  {
    error("No input field");
    return (false);
  }
  
  FieldInformation fo(input);
  
  const std::string basistype = get_option(Parameters::OutputType);
  const bool buildBasisMapping = get(Parameters::BuildBasisMapping).toBool();
  
 
  return (true);
}

bool
SwapFieldDataWithMatrixEntries::runImpl(FieldHandle input, FieldHandle& output) const
{
  MatrixHandle dummy;
  return runImpl(input,output,dummy);
}

AlgorithmOutput SwapFieldDataWithMatrixEntries::run_generic(const AlgorithmInput& input) const
{
  auto field = input.get<Field>(Variables::InputField);

  FieldHandle outputField;
  if (!runImpl(field, outputField))
    THROW_ALGORITHM_PROCESSING_ERROR("False returned on legacy run call.");

  AlgorithmOutput output;
  output[Variables::OutputField] = outputField;
  return output;
}
