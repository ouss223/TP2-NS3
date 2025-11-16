import pandas as pd
import matplotlib.pyplot as plt
import os

if os.path.exists('client_delays.csv'):
    df = pd.read_csv('client_delays.csv')
    plt.figure(figsize=(10, 6))
    plt.plot(df['PacketNumber'], df['DelayMs'], 'bo-', linewidth=2, markersize=8)
    plt.xlabel('Numéro de paquet')
    plt.ylabel('Délai bout-en-bout (ms)')
    plt.title('Délai requête-réponse en fonction du numéro de paquet\\n(' + str(4) + ' STA par réseau, ' + str(10) + ' paquets)')
    plt.grid(True, alpha=0.3)
    plt.xticks(range(1, 11))
    plt.tight_layout()
    plt.savefig('delai_bout_en_bout.png', dpi=300)
    print('Graphique sauvegardé: delai_bout_en_bout.png')
    plt.show()
else:
    print('Fichier de délais non trouvé: client_delays.csv')
