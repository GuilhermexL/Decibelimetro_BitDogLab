import serial
import time
import os

# CONFIGURAÇÕES
PORTA = 'COM7'           # Porta onde sua placa aparece no Windows
BAUD = 115200            # Velocidade de comunicação (deve bater com o código C)
ARQUIVO_LOG = 'logs_matriz_leds_rgb.txt'  # Nome do arquivo onde os logs serão salvos

# CRIA O ARQUIVO DE LOG (ou anexa se já existir)
if not os.path.exists(ARQUIVO_LOG):
    with open(ARQUIVO_LOG, 'w') as f:
        f.write("===== LOGS INICIADOS EM {} =====\n".format(time.strftime("%Y-%m-%d %H:%M:%S")))

try:
    with serial.Serial(PORTA, BAUD, timeout=1) as ser, open(ARQUIVO_LOG, 'a') as log_file:
        print(f"📡 Conectado à porta {PORTA} — aguardando dados...\n(Pressione CTRL+C para parar)\n")
        
        while True:
            linha = ser.readline().decode('utf-8', errors='ignore').strip()
            if linha:
                timestamp = time.strftime("[%Y-%m-%d %H:%M:%S]")
                log = f"{timestamp} {linha}"
                print(log)
                log_file.write(log + '\n')
                log_file.flush()

except serial.SerialException:
    print(f"❌ Erro: Não foi possível abrir a porta {PORTA}. Verifique se a placa está conectada.")
except KeyboardInterrupt:
    print("\n✅ Captura de logs finalizada pelo usuário.")
