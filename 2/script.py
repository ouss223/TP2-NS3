#!/usr/bin/env python3
# parse_tracemetrics_metrics.py
# Parse tracemetrics NS-3 and compute average throughput, packet loss and latency.
import sys, re
from collections import defaultdict, deque
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("Usage: python3 parse_tracemetrics_metrics.py PATH/TO/tracemetrics")
    sys.exit(1)
path = sys.argv[1]

# permissive regex
time_re = re.compile(r'(?P<time>\d+\.\d+)(?P<unit>ns|us|ms|s)?')
len_re = re.compile(r'(?:len|size|length|frame_len)[=:\s]*(\d+)')
ip_re = re.compile(r'(\d{1,3}(?:\.\d{1,3}){3})')
udp_port_re = re.compile(r'udp\.port[:=]?\s*(\d+)|udp.srcport[:=]?\s*(\d+)|udp.dstport[:=]?\s*(\d+)', re.IGNORECASE)
tcp_seq_re = re.compile(r'(?:tcp\.seq|seq)[=:\s]*(\d+)')

records = []
with open(path, errors='replace') as f:
    for line in f:
        line = line.strip()
        if not line: continue
        m = time_re.search(line)
        if not m: continue
        tval = float(m.group('time')); unit = m.group('unit')
        if unit=='ns': t = tval*1e-9
        elif unit=='us': t = tval*1e-6
        elif unit=='ms': t = tval*1e-3
        else: t = tval
        mlen = len_re.search(line)
        size = int(mlen.group(1)) if mlen else None
        ips = ip_re.findall(line)
        ip_src = ips[0] if len(ips)>=1 else None
        ip_dst = ips[1] if len(ips)>=2 else None
        # udp ports best-effort
        udp_sport=None; udp_dport=None
        for m2 in udp_port_re.finditer(line):
            for g in m2.groups():
                if g:
                    if udp_sport is None:
                        udp_sport = int(g)
                    elif udp_dport is None:
                        udp_dport = int(g)
        mseq = tcp_seq_re.search(line)
        seq = int(mseq.group(1)) if mseq else None
        records.append({'time':t,'size':size,'ip_src':ip_src,'ip_dst':ip_dst,'udp_sport':udp_sport,'udp_dport':udp_dport,'seq':seq,'raw':line})

if not records:
    print("Aucune ligne analysable.")
    sys.exit(1)

df = pd.DataFrame.from_records(records).sort_values('time').reset_index(drop=True)
t0 = df['time'].min(); t1 = df['time'].max()
df['t_rel'] = df['time'] - t0
df['size'] = df['size'].fillna(1024).astype(int)  # fallback

# --- débit moyen global ---
total_bytes = int(df['size'].sum())
duration = float(t1 - t0) if t1>t0 else 1.0
avg_throughput_mbps = (total_bytes * 8.0) / (duration * 1e6)

# --- tentative appairage UDP echo (port 9) pour perte + latence ---
reqs = [r for _,r in df.iterrows() if r['udp_dport']==9]
reps = [r for _,r in df.iterrows() if r['udp_sport']==9]

latencies = []
matched = 0
if reqs and reps:
    # index replies by (src,dst,size) -> queue
    rep_index = defaultdict(deque)
    for r in reps:
        key = (r['ip_src'], r['ip_dst'], int(r['size']))
        rep_index[key].append(r['time'])
    for r in reqs:
        key = (r['ip_dst'], r['ip_src'], int(r['size']))
        if key in rep_index and rep_index[key]:
            trep = rep_index[key].popleft()
            if trep >= r['time']:
                latencies.append(trep - r['time'])
                matched += 1
    loss_pct = (1.0 - (matched / len(reqs))) * 100.0 if len(reqs)>0 else None
    method = "udp-echo-port9"
else:
    # --- tcp.seq-gap heuristic ---
    seqs_by_flow = defaultdict(list)
    for _,r in df.dropna(subset=['seq']).iterrows():
        if r['ip_src'] and r['ip_dst']:
            seqs_by_flow[(r['ip_src'], r['ip_dst'])].append(int(r['seq']))
    total_lost = 0; total_sent = 0
    for flow, s in seqs_by_flow.items():
        ss = sorted(set(s))
        if len(ss) < 2:
            total_sent += len(ss); continue
        gaps = [ss[i+1]-ss[i] for i in range(len(ss)-1)]
        lost = sum((g-1) for g in gaps if g>1)
        total_lost += lost
        total_sent += len(ss)
    if total_sent + total_lost > 0:
        loss_pct = (total_lost / (total_sent + total_lost)) * 100.0
        method = "tcp-seq-gap"
    else:
        # --- fallback bytes ratio by heuristics of 'tx'/'rx' in raw lines ---
        tx_bytes = 0; rx_bytes = 0; tx_count=0; rx_count=0
        for _,r in df.iterrows():
            lo = (r['raw'] or '').lower()
            if 'tx' in lo or 'send' in lo or 'transmit' in lo:
                tx_bytes += int(r['size']); tx_count += 1
            elif 'rx' in lo or 'recv' in lo or 'receive' in lo:
                rx_bytes += int(r['size']); rx_count += 1
        if tx_bytes>0:
            loss_pct = (1.0 - (rx_bytes/tx_bytes))*100.0
            method = "bytes-ratio-fallback"
        elif tx_count>0:
            loss_pct = (1.0 - (rx_count/tx_count))*100.0
            method = "count-ratio-fallback"
        else:
            loss_pct = None
            method = "unknown"

# --- latency summary if any ---
lat_summary = None
if latencies:
    lat_ms = np.array(latencies) * 1000.0
    lat_summary = {
        'mean_ms': float(lat_ms.mean()),
        'median_ms': float(np.median(lat_ms)),
        'p95_ms': float(np.percentile(lat_ms,95)),
        'samples': len(lat_ms)
    }
    # save histogram
    plt.figure(figsize=(6,3))
    plt.hist(lat_ms, bins=40)
    plt.xlabel("Latence (ms)")
    plt.ylabel("Compte")
    plt.title("Histogramme des latences")
    plt.tight_layout()
    plt.savefig("latency_hist.png")

# --- sauvegarde throughput time series (graphique existant) ---
win = 0.5
tmin = df['t_rel'].min(); tmax = df['t_rel'].max()
bins = np.arange(tmin, tmax + win, win)
byte_counts, _ = np.histogram(df['t_rel'], bins=bins, weights=df['size'])
centers = (bins[:-1] + bins[1:]) / 2.0
throughput_mbps = (byte_counts * 8.0) / (win * 1e6)
plt.figure(figsize=(10,4))
plt.plot(centers, throughput_mbps, marker='o', linewidth=1)
plt.xlabel("Temps relatif (s)")
plt.ylabel("Débit (Mbps)")
plt.title(f"Débit estimé (fenêtre={win}s)")
plt.grid(True)
plt.tight_layout()
plt.savefig("throughput_all.png")

# --- counts per flow (graphique existant) ---
if df['ip_src'].notna().any() and df['ip_dst'].notna().any():
    flow_counts = df.dropna(subset=['ip_src','ip_dst']).groupby(['ip_src','ip_dst']).size().reset_index(name='count')
    flow_counts = flow_counts.sort_values('count', ascending=False).head(30)
    labels = [f"{a}→{b}" for a,b in flow_counts[['ip_src','ip_dst']].values]
    plt.figure(figsize=(10,4))
    plt.bar(range(len(labels)), flow_counts['count'])
    plt.xticks(range(len(labels)), labels, rotation=45, ha='right')
    plt.ylabel("Nombre de paquets")
    plt.title("Top flows (paquets)")
    plt.tight_layout()
    plt.savefig("counts_all.png")

# --- imprimés finaux et enregistrement métriques ---
print("\n--- MÉTRIQUES ---")
print(f"Période: {duration:.3f} s (de {t0:.6f} à {t1:.6f})")
print(f"Débit moyen (global) : {avg_throughput_mbps:.6f} Mbps (total {total_bytes} B)")
if loss_pct is not None:
    print(f"Perte estimée : {loss_pct:.3f}%  (méthode: {method})")
else:
    print("Perte estimée : impossible (pas d'heuristique applicable)")
if lat_summary:
    print(f"Latence (ms) : mean={lat_summary['mean_ms']:.2f}, median={lat_summary['median_ms']:.2f}, p95={lat_summary['p95_ms']:.2f} (n={lat_summary['samples']})")
else:
    print("Latence : aucune paire request→reply détectée (réaliser extraction pcap si nécessaire)")

# save textual metrics
with open("metrics_summary.txt","w") as out:
    out.write(f"duration_s,{duration:.6f}\n")
    out.write(f"total_bytes,{total_bytes}\n")
    out.write(f"avg_throughput_mbps,{avg_throughput_mbps:.6f}\n")
    out.write(f"loss_pct,{loss_pct if loss_pct is not None else 'NA'}\n")
    if lat_summary:
        out.write(f"lat_mean_ms,{lat_summary['mean_ms']:.3f}\n")
        out.write(f"lat_median_ms,{lat_summary['median_ms']:.3f}\n")
        out.write(f"lat_p95_ms,{lat_summary['p95_ms']:.3f}\n")
    else:
        out.write("lat_mean_ms,NA\nlat_median_ms,NA\nlat_p95_ms,NA\n")

print("Fichiers produits: throughput_all.png, counts_all.png, latency_hist.png (si latences), metrics_summary.txt")
