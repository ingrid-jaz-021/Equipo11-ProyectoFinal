import math

vertices = []
cemp_vindices = []

current_mat = None
with open("Proyecto/Models/EntornoCompleto/EntornoCompleto.obj", "r", encoding="utf-8", errors="ignore") as f:
    for line in f:
        if line.startswith("v "):
            parts = line.split()
            vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif line.startswith("usemtl "):
            current_mat = line.split()[1].strip()
        elif line.startswith("f "):
            if current_mat == "FlorCempasuchil.003":
                for part in line.split()[1:]:
                    v_idx = int(part.split('/')[0]) - 1
                    cemp_vindices.append(v_idx)

pts = [vertices[i] for i in set(cemp_vindices)]
print(f"Total flower vertices: {len(pts)}")

# Cluster points by proximity in XZ (e.g. radius 2.0)
clusters = []
for p in pts:
    matched = False
    for c in clusters:
        dx = p[0] - c['avg'][0]
        dz = p[2] - c['avg'][2]
        if dx*dx + dz*dz < 9.0: # within 3 units
            c['pts'].append(p)
            # update avg periodically
            if len(c['pts']) % 20 == 0:
                n = len(c['pts'])
                c['avg'] = (
                    sum(pt[0] for pt in c['pts'])/n,
                    sum(pt[1] for pt in c['pts'])/n,
                    sum(pt[2] for pt in c['pts'])/n
                )
            matched = True
            break
    if not matched:
        clusters.append({'pts': [p], 'avg': p})

print(f"Found {len(clusters)} flower clusters:")
clusters_sorted = sorted(clusters, key=lambda c: (-c['avg'][2], c['avg'][0]))
for i, c in enumerate(clusters_sorted):
    min_x = min(pt[0] for pt in c['pts'])
    max_x = max(pt[0] for pt in c['pts'])
    min_y = min(pt[1] for pt in c['pts'])
    max_y = max(pt[1] for pt in c['pts'])
    min_z = min(pt[2] for pt in c['pts'])
    max_z = max(pt[2] for pt in c['pts'])
    cx = (min_x + max_x) / 2
    cy = min_y # ground base
    cz = (min_z + max_z) / 2
    span_x = max_x - min_x
    span_y = max_y - min_y
    span_z = max_z - min_z
    print(f"Flower {i:2d}: count={len(c['pts']):5d} pos=({cx:7.2f}, {cy:5.2f}, {cz:7.2f}) span=({span_x:4.2f}, {span_y:4.2f}, {span_z:4.2f})")

