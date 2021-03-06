/*****************************************************************************
*
* Copyright (c) 2000 - 2018, Lawrence Livermore National Security, LLC
* Produced at the Lawrence Livermore National Laboratory
* LLNL-CODE-442911
* All rights reserved.
*
* This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
* full copyright notice is contained in the file COPYRIGHT located at the root
* of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
*
* Redistribution  and  use  in  source  and  binary  forms,  with  or  without
* modification, are permitted provided that the following conditions are met:
*
*  - Redistributions of  source code must  retain the above  copyright notice,
*    this list of conditions and the disclaimer below.
*  - Redistributions in binary form must reproduce the above copyright notice,
*    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
*    documentation and/or other materials provided with the distribution.
*  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
* ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
* LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
* DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
* SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
* CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
* LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
* OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*****************************************************************************/

// ************************************************************************* //
//                            avtADIOSFileFormat.C                           //
// ************************************************************************* //

#include <avtMTMDFileFormatInterface.h>
#include <avtParallel.h>
#include "avtXGCFileFormat.h"
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <avtDatabaseMetaData.h>
#include <DBOptionsAttributes.h>
#include <InvalidDBTypeException.h>
#include <InvalidVariableException.h>
#include <DebugStream.h>
#include <avtDatabase.h>
#include <vtkUnstructuredGrid.h>
#include <vtkTriangle.h>
#include <sstream>
#include <fstream>
#include <VisItStreamUtil.h>

#include <vtkCellLocator.h>
#include <vtkGenericCell.h>

using namespace std;

static vector<double>
interpolate(int n, double *x, double *y, int ni, double *xi);

// ****************************************************************************
//  Method: avtEMSTDFileFormat::Identify
//
//  Purpose:
//      Determine if this file is of this flavor.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

bool
avtXGCFileFormat::Identify(const char *fname)
{
    adios2::ADIOS adios;
    adios2::IO io(adios.DeclareIO("ReadBP"));
    adios2::Engine reader = io.Open(fname, adios2::Mode::Read);
    debug1 << "avtXGCFileFormat::GetMesh " << fname << endl;

    std::map<std::string, adios2::Params> variables, attributes;
    variables = io.AvailableVariables();
    attributes = io.AvailableAttributes();

    int vfind = 0;
    vector<string> reqVars = {"dpot"};
    for (auto vi = variables.begin(); vi != variables.end(); vi++)
        if (std::find(reqVars.begin(), reqVars.end(), vi->first) != reqVars.end())
            return true;

    return vfind==reqVars.size();
    // string str(fname);
    // if (str.find("xgc.") != string::npos)
    //     return true;
    // else if (str.find("xgc.particle") != string::npos)
    //     return true;
    
    // return false;
}


// ****************************************************************************
//  Method: avtEMSTDFileFormat::CreateInterface
//
//  Purpose:
//      Create an interface for this reader.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

avtFileFormatInterface *
avtXGCFileFormat::CreateInterface(const char *const *list,
                                  int nList,
                                  int nBlock)
{
    int nTimestepGroups = nList / nBlock;
    avtMTMDFileFormat **ffl = new avtMTMDFileFormat*[nTimestepGroups];
    for (int i = 0 ; i < nTimestepGroups ; i++)
        ffl[i] = new avtXGCFileFormat(list[i*nBlock]);
    auto *tmp = new avtMTMDFileFormatInterface(ffl, nTimestepGroups);
    cout << "create interface" << endl;
    return tmp;
}

// ****************************************************************************
//  Method: avtEMSTDFileFormat::CreateMeshName
//
//  Purpose:
//      Form the mesh name for this file.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

string
avtXGCFileFormat::CreateMeshName(const string &filename)
{
    string meshname = "";
    
    string::size_type i0 = filename.rfind("xgc.");
    string::size_type i1 = filename.rfind(".bp");
    
    if (i0 != string::npos && i1 != string::npos)
    {
        meshname = filename.substr(0,i0+4) + "mesh.bp";
    }
    return meshname;
}

string
avtXGCFileFormat::CreateDiagName(const string &filename)
{
    string diagname = "";
    
    string::size_type i0 = filename.rfind("xgc.");
    string::size_type i1 = filename.rfind(".bp");
    
    if (i0 != string::npos && i1 != string::npos)
    {
        diagname = filename.substr(0,i0+4) + "oneddiag.bp";
    }
    return diagname;
}

string
avtXGCFileFormat::CreateSeparatrixName(const string &filename)
{
    string sepname = "";
    
    string::size_type i0 = filename.rfind("xgc.");
    string::size_type i1 = filename.rfind(".bp");
    
    if (i0 != string::npos && i1 != string::npos)
    {
        sepname = filename.substr(0,i0) + "sep.dat";
    }
    return sepname;
}

// ****************************************************************************
//  Method: avtXGCFileFormat constructor
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

avtXGCFileFormat::avtXGCFileFormat(const char *nm)
    : file(std::make_shared<adios2::ADIOS>(adios2::DebugON)),
     avtMTMDFileFormat(nm),
     filename(nm)
{
    engineType = "BP";
    fileIO = adios2::IO(file->DeclareIO(engineType));
    fileIO.SetEngine(engineType);

    fileReader = fileIO.Open(std::string(filename), adios2::Mode::Read);
    if (!fileReader)
        EXCEPTION1(ImproperUseException, "Invalid file");

    variables = fileIO.AvailableVariables();
    attributes = fileIO.AvailableAttributes();
    if (variables.find("pot0") != variables.end())
        numTimeSteps = std::stoi(variables["pot0"]["AvailableStepsCount"]);

    initialized = false;
    numNodes = 0;

    numTris = 0;

    numPhi = 0;
    // file = new ADIOSFileObject(nm);
    // file->SetResetDimensionOrder();
    
    // meshFile = NULL;
    // diagFile = NULL;
    // initialized = false;
    // haveSepMesh = false;
    // numNodes = 0;
    // numTris = 0;
    // numPhi = 0;
    
    //string str(nm);
}

// ****************************************************************************
//  Method: avtXGCFileFormat destructor
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

avtXGCFileFormat::~avtXGCFileFormat()
{
}


// ****************************************************************************
//  Method: avtEMSTDFileFormat::GetNTimesteps
//
//  Purpose:
//      Tells the rest of the code how many timesteps there are in this file.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

int
avtXGCFileFormat::GetNTimesteps()
{
    return numTimeSteps;
}

// ****************************************************************************
//  Method: avtEMSTDFileFormat::GetCycles
//
//  Purpose:
//      
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

void
avtXGCFileFormat::GetCycles(vector<int> &cycles)
{
    cycles.resize(numTimeSteps);
    for (int i = 0; i < numTimeSteps; i++)
        cycles[i] = i;

}


// ****************************************************************************
//  Method: avtXGCFileFormat::FreeUpResources
//
//  Purpose:
//      When VisIt is done focusing on a particular timestep, it asks that
//      timestep to free up any resources (memory, file descriptors) that
//      it has associated with it.  This method is the mechanism for doing
//      that.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

void
avtXGCFileFormat::FreeUpResources(void)
{
}


// ****************************************************************************
//  Method: avtXGCFileFormat::PopulateDatabaseMetaData
//
//  Purpose:
//      This database meta-data object is like a table of contents for the
//      file.  By populating it, you are telling the rest of VisIt what
//      information it can request from you.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

void
avtXGCFileFormat::PopulateDatabaseMetaData(avtDatabaseMetaData *md, int timeState)
{
    debug5<<"avtXGCFileFormat::PopulateDatabaseMetaData()"<<endl;
    Initialize();
    md->SetFormatCanDoDomainDecomposition(false);


    //Mesh.
    avtMeshMetaData *mesh = new avtMeshMetaData;
    mesh->name = "mesh";
    mesh->meshType = AVT_UNSTRUCTURED_MESH;
    mesh->numBlocks = 1;
    mesh->blockOrigin = 0;
    mesh->spatialDimension = 3;
    mesh->topologicalDimension = 3;
    mesh->blockTitle = "blocks";
    mesh->blockPieceName = "block";
    mesh->hasSpatialExtents = false;
    md->Add(mesh);

    if (haveSepMesh)
    {
        AddMeshToMetaData(md, "sep_mesh", AVT_UNSTRUCTURED_MESH, NULL, 1, 0, 3, 3);
        AddScalarVarToMetaData(md, "sep", "sep_mesh", AVT_NODECENT);
    }

    //Add pot0....
    AddMeshToMetaData(md, "mesh2D", AVT_UNSTRUCTURED_MESH, NULL, 1, 0, 3, 3);
    AddScalarVarToMetaData(md, "pot0", "mesh2D", AVT_NODECENT);
    AddScalarVarToMetaData(md, "psi", "mesh2D", AVT_NODECENT);

    // map<string, ADIOS_VARINFO*>::const_iterator it;
    // for (it = file->variables.begin(); it != file->variables.end(); it++)
    // {
    //     ADIOS_VARINFO *avi = it->second;
    //     if (avi->ndim == 1)
    //         continue;

    for (auto vi = variables.begin(); vi != variables.end(); vi++)
    {
        if (std::atoi(vi->second["ndim"].c_str()) == 1)
            continue;

        avtScalarMetaData *smd = new avtScalarMetaData();
        smd->meshName = "mesh";
        smd->name = vi->first;
        if (smd->name[0] == '/')
        {
            smd->originalName = smd->name;
            smd->name = string(&smd->name[1]);
        }
        smd->centering = AVT_NODECENT;
        md->Add(smd);
    }
    
    if (diagFile.get())
        AddScalarVarToMetaData(md, "turbulence", "mesh", AVT_NODECENT);
}


// ****************************************************************************
//  Method: avtXGCFileFormat::GetMesh
//
//  Purpose:
//      Gets the mesh associated with this file.  The mesh is returned as a
//      derived type of vtkDataSet (ie vtkRectilinearGrid, vtkStructuredGrid,
//      vtkUnstructuredGrid, etc).
//
//  Arguments:
//      timestate   The index of the timestate.  If GetNTimesteps returned
//                  'N' time steps, this is guaranteed to be between 0 and N-1.
//      meshname    The name of the mesh of interest.  This can be ignored if
//                  there is only one mesh.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
//  Modifications:
//
//   Dave Pugmire, Tue Jun 17 13:24:30 EDT 2014
//   Bug fix to ADIOSFile interface.
//
// ****************************************************************************

vtkDataSet *
avtXGCFileFormat::GetMesh(int timestate, int domain, const char *meshname)
{
    debug1 << "avtXGCFileFormat::GetMesh " << meshname << endl;
    Initialize();
    // if (!strcmp(meshname, "mesh2D"))
    //     return GetMesh2D(timestate, domain);

    //meshFile->ReadScalarData("/coordinates/values", timestate, &buff);
    adios2::Variable<float> coordVar = fileIO.InquireVariable<float>("coordinates/values");
    if (!coordVar)
        return NULL;
    coordVar.SetStepSelection({timestate,1});
    vector<float> buff;
    vector<int> conn, nextNode;
    meshReader.Get(coordVar, buff, adios2::Mode::Sync);

    vtkPoints *pts = vtkPoints::New();
    pts->SetNumberOfPoints(numNodes * numPhi);
    vtkUnstructuredGrid *grid = vtkUnstructuredGrid::New();
    grid->SetPoints(pts);

    //vtkDataArray *conn = NULL, *nextNode = NULL;
    // meshFile->ReadScalarData("/cell_set[0]/node_connect_list", timestate, &conn);
    // if (!meshFile->ReadScalarData("/nextnode", timestate, &nextNode))
    //     meshFile->ReadScalarData("nextnode", timestate, &nextNode);
    auto nodeConnectorVar = fileIO.InquireVariable<int>("/cell_set[0]/node_connect_list");
    auto nextNodeVar = fileIO.InquireVariable<int>("nextnode");
    if (!nodeConnectorVar || !nextNodeVar)
        return NULL;
    nodeConnectorVar.SetStepSelection({timestate,1});
    nextNodeVar.SetStepSelection({timestate,1});

    meshReader.Get(nodeConnectorVar, conn,adios2::Mode::Sync);
    meshReader.Get(nextNodeVar, nextNode, adios2::Mode::Sync);
    if (nextNode.size() < 1)
        return NULL;

    //Create the points.
    double dPhi = 2.0*M_PI/(double)numPhi;
    for (int i = 0; i < numPhi; i++)
    {
        double phi = (double)i * dPhi;
        phi = -phi;
        
        double pt[3];
        for (int p = 0; p < numNodes; p++)
        {
            double R = buff[p*2 +0];
            double Z = buff[p*2 +1];
            
            pt[0] = R*cos(phi);
            pt[1] = R*sin(phi);
            pt[2] = Z;
            pts->SetPoint(p+i*numNodes, pt);
        }
    }
    pts->Delete();

    //Make the wedges.
    vtkIdType wedge[6];
    for (int i = 0; i < numPhi; i++)
    {
        for (int p = 0; p < numTris*3; p+=3)
        {
            int off = i*(numNodes);
            wedge[0] = conn[p+0] + off;
            wedge[1] = conn[p+1] + off;
            wedge[2] = conn[p+2] + off;

            off = (i+1)*(numNodes);
            int p0 = conn[p+0];
            int p1 = conn[p+1];
            int p2 = conn[p+2];

            p0 = nextNode[p0];
            p1 = nextNode[p1];
            p2 = nextNode[p2];

            //Connect back to the first plane.
            if (i == numPhi-1)
                off = 0;
            
            wedge[3] = p0 + off;
            wedge[4] = p1 + off;
            wedge[5] = p2 + off;
            grid->InsertNextCell(VTK_WEDGE, 6, wedge);
        }
    }
    return grid;
}



//****************************************************************************
// Method:  avtXGCFileFormat::GetMesh2D
//
// Purpose:
//   Read 2D mesh
//
// Programmer:  Dave Pugmire
// Creation:    June 17, 2014
//
// Modifications:
//
//   Dave Pugmire, Tue Jun 17 13:24:30 EDT 2014
//   Bug fix to ADIOSFile interface.
//
//****************************************************************************


vtkDataSet *
avtXGCFileFormat::GetMesh2D(int ts, int dom)
{
    // vtkDataArray *buff = NULL;
    // meshFile->ReadScalarData("/coordinates/values", ts, &buff);
    
    // vtkPoints *pts = vtkPoints::New();
    // pts->SetNumberOfPoints(numNodes);
    // vtkUnstructuredGrid *grid = vtkUnstructuredGrid::New();
    // grid->SetPoints(pts);

    // vtkDataArray *conn = NULL;
    // meshFile->ReadScalarData("/cell_set[0]/node_connect_list", ts, &conn);

    // //Create the points.
    // double dPhi = 2.0*M_PI/(double)numPhi;
    // double phi = 0.0;
    // double pt[3];
    // for (int p = 0; p < numNodes; p++)
    // {
    //     double R = buff->GetTuple1(p*2 +0);
    //     double Z = buff->GetTuple1(p*2 +1);
            
    //     /*
    //     pt[0] = R*cos(phi);
    //     pt[1] = R*sin(phi);
    //     pt[2] = Z;
    //     */

    //     pt[0] = R*cos(phi);
    //     pt[2] = R*sin(phi);
    //     pt[1] = Z;
    //     pts->SetPoint(p, pt);
    // }
    // buff->Delete();
    // pts->Delete();

    // //Make the wedges.
    // int *connPtr = (int *)(conn->GetVoidPointer(0));
    // vtkIdType tri[3];
    // for (int p = 0; p < numTris*3; p+=3)
    // {
    //     tri[0] = connPtr[p+0];
    //     tri[1] = connPtr[p+1];
    //     tri[2] = connPtr[p+2];
    //     grid->InsertNextCell(VTK_TRIANGLE, 3, tri);
    // }
    
    // conn->Delete();
    // return grid;
}




// ****************************************************************************
//  Method: avtXGCFileFormat::GetVar
//
//  Purpose:
//      Gets a scalar variable associated with this file.  Although VTK has
//      support for many different types, the best bet is vtkFloatArray, since
//      that is supported everywhere through VisIt.
//
//  Arguments:
//      timestate  The index of the timestate.  If GetNTimesteps returned
//                 'N' time steps, this is guaranteed to be between 0 and N-1.
//      varname    The name of the variable requested.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

vtkDataArray *
avtXGCFileFormat::GetVar(int timestate, int domain, const char *varname)
{
    debug1 << "avtXGCFileFormat::GetVar " << varname << endl;
    Initialize();

    if (!strcmp(varname, "psi"))
        return GetPsi();


    adios2::Variable<float> var = fileIO.InquireVariable<float>(varname);
    if (!var)
        return NULL;

    auto dims = var.Shape();

    vtkFloatArray *arr = vtkFloatArray::New();
    arr->SetNumberOfTuples(dims[0]*dims[1]);

    var.SetStepSelection({timestate, 1});
    var.SetSelection(adios2::Box<adios2::Dims>({0,0}, dims));
    fileReader.Get(var, (float*)arr->GetVoidPointer(0), adios2::Mode::Sync);

    return arr;
}


//****************************************************************************
// Method:  avtXGCFileFormat::GetPsi
//
// Purpose:
//   Read PSI from the mesh file.
//
// Programmer:  Dave Pugmire
// Creation:    October 29, 2014
//
// Modifications:
//
//****************************************************************************

vtkDataArray *
avtXGCFileFormat::GetPsi()
{
    vtkDataArray *psi;
    adios2::Variable<float> var = fileIO.InquireVariable<float>("psi");
    if (!var)
        return NULL;

    auto dims = var.Shape();

    psi->SetNumberOfTuples(dims[0]*dims[1]);

    var.SetSelection(adios2::Box<adios2::Dims>({0,0}, dims));
    fileReader.Get(var, (float*)psi->GetVoidPointer(0), adios2::Mode::Sync);

    // if (!meshFile->ReadScalarData("/psi", 0, &psi))
    //     meshFile->ReadScalarData("psi", 0, &psi);

    return psi;
}




// ****************************************************************************
//  Method: avtXGCFileFormat::GetVectorVar
//
//  Purpose:
//      Gets a vector variable associated with this file.  Although VTK has
//      support for many different types, the best bet is vtkFloatArray, since
//      that is supported everywhere through VisIt.
//
//  Arguments:
//      timestate  The index of the timestate.  If GetNTimesteps returned
//                 'N' time steps, this is guaranteed to be between 0 and N-1.
//      varname    The name of the variable requested.
//
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

vtkDataArray *
avtXGCFileFormat::GetVectorVar(int timestate, int domain, const char *varname)
{
    return NULL;
}

// ****************************************************************************
//  Method: avtXGCFileFormat::Initialize
//
//  Purpose:
//      Initialize the reader.
//
//  Programmer: Dave Pugmire
//  Creation:   Tue Mar  9 12:40:15 EST 2010
//
// ****************************************************************************

void
avtXGCFileFormat::Initialize()
{
    adios2::ADIOS adios;
    adios2::IO bpIO = adios.DeclareIO("ReadBP");
    adios2::Engine bpReader =
        bpIO.Open(filename.c_str(), adios2::Mode::Read);
    const std::map<std::string, adios2::Params> variables =
        bpIO.AvailableVariables();

    for (const auto variablePair : variables)
    {
        std::cout << "Name: " << variablePair.first;

        for (const auto &parameter : variablePair.second)
        {
            std::cout << "\t" << parameter.first << ": " << parameter.second
                        << "\n";
        }
    }

    
    // //Open the mesh file.
    // meshFile = new ADIOSFileObject(avtXGCFileFormat::CreateMeshName(file->Filename()));
    // meshFile->SetResetDimensionOrder();
    // if (! meshFile->Open())
    //     EXCEPTION0(ImproperUseException);
    meshFile->DeclareIO("ReadBP");
    meshIO.Open(avtXGCFileFormat::CreateMeshName(filename), adios2::Mode::Read);
    if (!meshIO)
         EXCEPTION0(ImproperUseException);           
    // string diagNm = avtXGCFileFormat::CreateDiagName(file->Filename());
    // ifstream df(diagNm.c_str());
    // if (df.good())
    // {
    //     diagFile = new ADIOSFileObject(diagNm);
    //     diagFile->SetResetDimensionOrder();
    //     if (! diagFile->Open())
    //         EXCEPTION0(ImproperUseException);
    // }

    // //See if we have a separtrix mesh file.
    // sepFileName = avtXGCFileFormat::CreateSeparatrixName(file->Filename());
    // ifstream f(sepFileName.c_str());
    // haveSepMesh = f.good();

    // //Read in mesh/plane info.
    // if (!meshFile->GetScalar("/n_n", numNodes))
    //     meshFile->GetScalar("n_n", numNodes);
    // if (!meshFile->GetScalar("/n_t", numTris))
    //     meshFile->GetScalar("n_t", numTris);
    // if (!file->GetScalar("/nphi", numPhi))
    //     file->GetScalar("nphi", numPhi);

    adios2::Variable<int> nVar = meshIO.InquireVariable<int>("n_n");
    adios2::Variable<int> triVar = meshIO.InquireVariable<int>("n_n");
    adios2::Variable<int> phiVar = meshIO.InquireVariable<int>("nphi");

    if (nVar){
        meshReader.Get(nVar, &numNodes, adios2::Mode::Sync);

    }
    if (triVar)
        meshReader.Get(triVar, &numTris, adios2::Mode::Sync);

    if (phiVar)
        meshReader.Get(phiVar, &numPhi, adios2::Mode::Sync);


    initialized = true;
}

//****************************************************************************
// Method:  interpolate
//
// Purpose:
//   calculate yi (from xi) based x and y.
//
// Programmer:  Dave Pugmire
// Creation:    April  9, 2014
//
// Modifications:
//
//****************************************************************************

static vector<double>
interpolate(int n, double *x, double *y, int ni, double *xi)
{
    vector<double> yi(ni);
    
    for (int i = 0; i < ni; i++)
    {
        yi[i] = 0.0;
        double val = xi[i];
        if (val <= x[0])
        {
            yi[i] = y[0];
            continue;
        }
        else if ( val >= x[n-1])
        {
            yi[i] = y[n-1];
            continue;
        }
        for (int j = 0; j < n-1; j++)
        {
            if (val >= x[j] && val <= x[j+1])
            {
                double dy = y[j+1]-y[j];
                double t = (xi[i]-x[j])/(x[j+1]-x[j]);
                yi[i] = y[j] + t*dy;
                break;
            }
        }
    }

    return yi;
}
