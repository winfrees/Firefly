"""Import a Freerouting .ses into a board (pcbnew 7 standalone lacks this)."""
import pcbnew
from sexp import parse, find, first


def import_ses(b, path):
    t = parse(open(path).read())
    routes = first(t, "routes")
    res = first(routes, "resolution")
    scale = {"um": 1000, "mm": 1000000, "mil": 25400}[res[1]] / float(res[2])  # nm per unit
    nets = {n.GetNetname(): n for n in b.GetNetsByName().values()}
    layer = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}
    nw = nv = 0
    vds = b.GetDesignSettings().m_NetSettings.m_DefaultNetClass
    for net in find(first(routes, "network_out"), "net"):
        ni = nets[net[1]]
        for w in find(net, "wire"):
            path = first(w, "path")
            ly, width = layer[path[1]], int(float(path[2]) * scale)
            c = [float(v) for v in path[3:]]
            pts = [(int(c[i] * scale), int(-c[i + 1] * scale)) for i in range(0, len(c), 2)]
            for a, z in zip(pts, pts[1:]):
                tr = pcbnew.PCB_TRACK(b)
                tr.SetStart(pcbnew.VECTOR2I(*a)); tr.SetEnd(pcbnew.VECTOR2I(*z))
                tr.SetWidth(width); tr.SetLayer(ly); tr.SetNet(ni)
                b.Add(tr); nw += 1
        for v in find(net, "via"):
            x, y = int(float(v[2]) * scale), int(-float(v[3]) * scale)
            via = pcbnew.PCB_VIA(b)
            via.SetPosition(pcbnew.VECTOR2I(x, y))
            via.SetWidth(vds.GetViaDiameter()); via.SetDrill(vds.GetViaDrill())
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); via.SetNet(ni)
            b.Add(via); nv += 1
    return nw, nv
