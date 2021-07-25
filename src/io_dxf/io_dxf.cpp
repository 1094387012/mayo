/****************************************************************************
** Copyright (c) 2021, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "io_dxf.h"

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Precision.hxx>
#include <gp_Trsf.hxx>

#include <string_view>

namespace Mayo {
namespace IO {

namespace {

Handle_Geom_BSplineCurve createSplineFromPolesAndKnots(struct SplineData& sd)
{
    const size_t numPoles = sd.control_points;
    if (sd.controlx.size() > numPoles || sd.controly.size() > numPoles ||
        sd.controlz.size() > numPoles || sd.weight.size() > numPoles) {
        return nullptr;
    }

    // handle the poles
    TColgp_Array1OfPnt occpoles(1, sd.control_points);
    int index = 1;
    for (auto x : sd.controlx)
        occpoles(index++).SetX(x);

    index = 1;
    for (auto y : sd.controly)
        occpoles(index++).SetY(y);

    index = 1;
    for (auto z : sd.controlz)
        occpoles(index++).SetZ(z);

    // handle knots and mults
    std::set<double> unique;
    unique.insert(sd.knot.begin(), sd.knot.end());

    const int numKnots = int(unique.size());
    TColStd_Array1OfInteger occmults(1, numKnots);
    TColStd_Array1OfReal occknots(1, numKnots);
    index = 1;
    for (auto k : unique) {
        const size_t m = std::count(sd.knot.begin(), sd.knot.end(), k);
        occknots(index) = k;
        occmults(index) = m;
        index++;
    }

    // handle weights
    TColStd_Array1OfReal occweights(1, sd.control_points);
    if (sd.weight.size() == size_t(sd.control_points)) {
        index = 1;
        for (auto w : sd.weight)
            occweights(index++) = w;
    }
    else {
        // non-rational
        for (int i = occweights.Lower(); i <= occweights.Upper(); i++)
            occweights(i) = 1.0;
    }

    const bool periodic = sd.flag == 2;
    return new Geom_BSplineCurve(occpoles, occweights, occknots, occmults, sd.degree, periodic);
}

Handle_Geom_BSplineCurve createInterpolationSpline(struct SplineData& sd)
{
    const size_t numPoints = sd.fit_points;
    if (sd.fitx.size() > numPoints || sd.fity.size() > numPoints || sd.fitz.size() > numPoints)
        return nullptr;

    // handle the poles
    Handle_TColgp_HArray1OfPnt fitpoints = new TColgp_HArray1OfPnt(1, sd.fit_points);
    int index = 1;
    for (auto x : sd.fitx)
        fitpoints->ChangeValue(index++).SetX(x);

    index = 1;
    for (auto y : sd.fity)
        fitpoints->ChangeValue(index++).SetY(y);

    index = 1;
    for (auto z : sd.fitz)
        fitpoints->ChangeValue(index++).SetZ(z);

    const bool periodic = sd.flag == 2;
    GeomAPI_Interpolate interp(fitpoints, periodic, Precision::Confusion());
    interp.Perform();
    return interp.Curve();
}

} // namespace

bool DxfReader::readFile(const FilePath& filepath, TaskProgress* progress)
{
    return false;
}

TDF_LabelSequence DxfReader::transfer(DocumentPtr doc, TaskProgress* progress)
{
    return {};
}

void DxfReader::OnReadLine(const double* s, const double* e, bool hidden)
{
    const gp_Pnt p0 = this->toPnt(s);
    const gp_Pnt p1 = this->toPnt(e);
    if (p0.IsEqual(p1, Precision::Confusion()))
        return;

    const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p0, p1);
    this->addShape(edge);
}

void DxfReader::OnReadPoint(const double* s)
{
    const TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(this->toPnt(s));
    this->addShape(vertex);
}

void DxfReader::OnReadText(const double* point, const double height, const char* text)
{
    if (m_importAnnotations)
        return;

    const gp_Pnt pt = this->toPnt(point);
#if 0
    if(LayerName().substr(0, 6) != "BLOCKS") {
        App::Annotation *pcFeature = (App::Annotation *)document->addObject("App::Annotation", "Text");
        pcFeature->LabelText.setValue(Deformat(text));
        pcFeature->Position.setValue(pt);
    }
    //else std::cout << "skipped text in block: " << LayerName() << std::endl;
#endif
}

void DxfReader::OnReadArc(const double* s, const double* e, const double* c, bool dir, bool hidden)
{
    const gp_Pnt p0 = this->toPnt(s);
    const gp_Pnt p1 = this->toPnt(e);
    const gp_Dir up = dir ? gp::DZ() : -gp::DZ();
    const gp_Pnt pc = this->toPnt(c);
    const gp_Circ circle(gp_Ax2(pc, up), p0.Distance(pc));
    if (circle.Radius() > 0) {
        const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circle, p0, p1);
        this->addShape(edge);
    }
    else {
        //Base::Console().Warning("ImpExpDxf - ignore degenerate arc of circle\n");
    }
}

void DxfReader::OnReadCircle(const double* s, const double* c, bool dir, bool hidden)
{
    const gp_Pnt p0 = this->toPnt(s);
    const gp_Dir up = dir ? gp::DZ() : -gp::DZ();
    const gp_Pnt pc = this->toPnt(c);
    const gp_Circ circle(gp_Ax2(pc, up), p0.Distance(pc));
    if (circle.Radius() > 0) {
        const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circle, p0, p1);
        this->addShape(edge);
    }
    else {
        //Base::Console().Warning("ImpExpDxf - ignore degenerate circle\n");
    }
}

void DxfReader::OnReadEllipse(const double* c, double major_radius, double minor_radius, double rotation, double start_angle, double end_angle, bool dir)
{
    const gp_Dir up = dir ? gp::DZ() : -gp::DZ();
    const gp_Pnt pc = this->toPnt(c);
    gp_Elips ellipse(
                gp_Ax2(pc, up),
                major_radius * m_params.scaling,
                minor_radius * m_params.scaling);
    ellipse.Rotate(gp_Ax1(pc, up), rotation);
    if (ellipse.MinorRadius() > 0) {
        const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(ellipse);
        this->addShape(edge);
    }
    else {
        //Base::Console().Warning("ImpExpDxf - ignore degenerate ellipse\n");
    }
}

void DxfReader::OnReadSpline(SplineData& sd)
{
    // https://documentation.help/AutoCAD-DXF/WS1a9193826455f5ff18cb41610ec0a2e719-79e1.htm
    // Flags:
    // 1: Closed, 2: Periodic, 4: Rational, 8: Planar, 16: Linear

    try {
        Handle_Geom_BSplineCurve geom;
        if (sd.control_points > 0)
            geom = createSplineFromPolesAndKnots(sd);
        else if (sd.fit_points > 0)
            geom = createInterpolationSpline(sd);

        if (geom.IsNull())
            throw Standard_Failure();

        const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(geom);
        this->addShape(edge);
    }
    catch (const Standard_Failure&) {
        //Base::Console().Warning("ImpExpDxf - failed to create bspline\n");
    }
}

void DxfReader::OnReadInsert(const double* point, const double* scale, const char* name, double rotation)
{
    //std::cout << "Inserting block " << name << " rotation " << rotation << " pos " << point[0] << "," << point[1] << "," << point[2] << " scale " << scale[0] << "," << scale[1] << "," << scale[2] << std::endl;
    const std::string prefix = std::string("BLOCKS ") + name + " ";
    for (const auto& [k, vecShape] : m_layers) {
        if (std::string_view(k).substr(0, prefix.size()) != prefix)
            continue; // Skip

        BRep_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);
        for (const TopoDS_Shape& shape : vecShape) {
            if (!shape.IsNull())
                builder.Add(comp, shape);
        }

        if (comp.IsNull())
            continue; // Skip

        gp_Trsf trsfScale;
        trsfScale.SetValues(
                scale[0], 0,        0,        0,
                0,        scale[1], 0,        0,
                0,        0,        scale[2], 0
                );
        gp_Trsf trsfRotZ;
        trsfRotZ.SetRotation(gp::OZ(), rotation);
        gp_Trsf trsfMove;
        trsfMove.SetTranslation(this->toPnt(point).XYZ());
        const gp_Trsf trsf = trsfScale * trsfRotZ * trsfMove;
        comp.Location(trsf);
        this->addShape(comp);
    }
}

void DxfReader::OnReadDimension(const double* s, const double* e, const double* point, double rotation)
{

}

void DxfReader::AddGraphics() const
{

}

gp_Pnt DxfReader::toPnt(const double* coords) const
{
    double sp1(coords[0]);
    double sp2(coords[1]);
    double sp3(coords[2]);
    if (m_params.scaling != 1.0) {
        sp1 = sp1 * m_params.scaling;
        sp2 = sp2 * m_params.scaling;
        sp3 = sp3 * m_params.scaling;
    }

    return gp_Pnt(sp1, sp2, sp3);
}

void DxfReader::addShape(const TopoDS_Shape& shape)
{
    const std::string layerName = this->LayerName();
    auto itFound = m_layers.find(layerName);
    if (itFound != m_layers.end()) {
        std::vector<TopoDS_Shape>& vecShape = itFound->second;
        vecShape.push_back(shape);
    }

    if (!m_groupLayers) {
        if(LayerName().substr(0, 6) != "BLOCKS") {
            Part::Feature *pcFeature = (Part::Feature *)document->addObject("Part::Feature", "Shape");
            pcFeature->Shape.setValue(shape->getShape());
        }
    }
}


} // namespace IO
} // namespace Mayo
