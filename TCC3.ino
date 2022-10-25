// SISTEMA SUPERVISÓRIO DE RECURSOS NATURAIS PARA RESIDÊNCIAS
// AUTORES: CRISTIANO FREESE, LEANDRO H. DEPINÉ, TIAGO VECCHI
// DATA : 14/02/2017
// Versão 1.1: Alteração das saídas
// Utilizando Placa Arduino Mega 2560 com shield Ethernet baseado no chipset Wiznet

// Este programa irá integrar 3 funcionalidades de otimização de recursos naturais para residências que serão executadas como funções
// para fornecimento de informações de status e alarmes para uma interface utilizando aplicativo Android via shield Ethernet ou página HTML.
// Além desta interface haverá um led RGB que fornecerá um status simplificado do sistema no qual uma cor irá indicar uma falha (pisca rápido)
// ou advertência (pisca lento) de uma das funcionalidades ou status geral ok (aceso RGB constante)
// As funcionalidades são:
 
// 1 - SUPERVISÃO PARA SISTEMA DE AQUECIMENTO SOLAR PARA ÁGUA - Led Vermelho
// Fornece funcionalidades de um controlador diferencial de temperatura para sistema de aquecimento de água
// por radiação solar. Ele possui as seguintes características:
//    1) Controle diferencial de temperatura
//    2) Proteção contra congelamento das placas
//    3) Proteção contra sobreaquecimento das placas
//    4) Proteção contra sobreaquecimento do reservatório
//    5) Relógio em tempo real
//    6) Programação de horário para ativar apoio elétrico via resistência
//      6.1) Controle de temperatura para apoio elétrico
//    7) Status de sobrecarga da Bomba de calor

// 2 - SUPERVISÃO PARA REAPROVEITAMENTO DE ÁGUA DA CHUVA - Led Azul
// Possui as seguintes características:
//    1) Sistema de enchimento de cisterna
//    2) Caixa superior deve ficar sempre cheia para melhor aproveitamento da água da chuva
//    3) Bóias de nível modelo on/off com histerese integrada. 
//    4) Composto por 3 bóias, uma bomba e uma válvula.
//    5) Bóia para cisterna apenas no nível inferior
//    6) Caixa superior com bóia nível máximo para ligar e desligar bomba da cisterna
//    7) Caixa superior com bóia de nível inferior para habilitar a válvula que libera água da rede na falta de água na cisterna
//    8) Resistor de pull_up nas entradas, portanto, as bóias enviam sinal baixo, 0Vcc
//    9) Possui status de sobrecarga para a bomba de recalque

// 3 - STATUS PARA CONTROLADOR CONTROLADOR DE CARGA - ENERGIA SOLAR - Led Verde
// Fornece dados externos de tensão e corrente utilizados por um controlador de carga para energia solar, sendo eles:
//    1) Tensão fornecida pelo painel fotovoltaico
//    2) Tensão da bateria estacionária
//    3) Corrente e Tensão de carga do sistema. Com estes dados é possível definir a potência de carga.
//    4) Status SobreCarga do sistema
//    5) Status Sem tensão na saída
//    6) Status de Sobretensão e Subtensão na bateria
//    7) Acionamento Fonte auxiliar AC/DC caso não haja tensão na saída do controlador de carga

// 4 - LED STATUS
// Fornece informação visual ao usuário diretamente na placa do sistema, permitindo a identificação rápido de uma falha ou advertência.
//    1) Led azul pisca lento - Reaproveitamento de água da chuva - Válvula ligada, consumindo água da rede
//    2) Led azul pisca rápido - Reaproveitamento de água da chuva - Falha na Bomba de Recalque
//    3) Led verde pisca lento - Energia solar - Acionamento da fonte auxiliar, devido a falta de tensão do sistema fotovoltaico
//    4) Led verde pisca rápido - Energia solar - Bateria danificada
//    5) Led vermelho pisca lento - Aquecimento solar - Resistência de apoio ligada, consumindo energia convencional
//    6) Led vermelho pisca rápido - Aquecimento solar - Falha na Bomba de Calor


// Definição Bibliotecas
#include <virtuabotixRTC.h>     // Biblioteca necessária para utilizaçao do módulo RTC      
#include <SPI.h>                // Biblioteca necessária para comunicação Ethernet
#include <Ethernet.h>           // Biblioteca necessária para comunicação Ethernet

//Endereço shield Ethernet
byte mac[]={0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};  //Endereço mac
byte ip[]={192,168,0,21};       //Endereço IP da placa arduino na rede
EthernetServer server(8090);    //porta de serviço do roteador


// Identificação dos pinos de entrada e saída e pinos utilizados em shields
// Aquecimento Solar
#define Sensor1       7         // Sensor de temperatura PT100 - Placas solares - -50°C .. 200°C
#define Sensor2       1         // Sensor de temperatura PT100 - Reservatório - -50°C .. 105°C
#define BombaCalor    22        // Bomba de calor e/ou recirculação da água
#define Resistencia   23        // Resistencia de apoio do reservatório
#define TermicoBombaCalor 36    // Relé térmico para a bomba de calor
#define RST_RTC       8         // Pino reset módulo RTC
#define DAT_RTC       7         // Pino data módulo RTC
#define CLK_RTC       6         // Pino clock módulo RTC
// Controlador de carga
#define TensaoCarga   2         // Tensão de carga do sistema
#define TensaoPainel  3         // Tensão fornecida pelo painel fotovoltaico
#define TensaoBateria 4         // Tensão da bateria estacionária
#define CorrenteCarga 8         // Corrente de carga do sistema
#define ReleFonteAuxiliar 24    // Liga a fonte auxiliar AC-DC caso não haja tensão da saída do controlador
// Reaproveitamento de Água da chuva
#define Boia_cisterna 25        // Bóia da cisterna
#define Caixa_max     26        // Bóia da caixa que controla nível alto 
#define Caixa_min     27        // Bóia da caixa que controla nível baixo
#define BombaRecalque 28        // Bomba de elevação da água
#define Valvula       29        // Válvula para água limpa para quando não tem água da chuva
#define TermicoBombaRecalque 34 // Relé térmico para a bomba de calor.  
// Uso comum
#define LedAquecimento 31       // Led vermelho do RGB indicando status da função de aquecimento solar.  
#define LedControlador 32       // Led verde do RGB indicando status da função de aquecimento solar.  
#define LedAgua        33       // Led azul do RGB indicand status da função de reaproveitamento da água.  

// Declaração de variáveis globais e constantes
// Aquecimento Solar
virtuabotixRTC Relogio(CLK_RTC, DAT_RTC, RST_RTC);    // Definição dos pinos utilizados para o modulo RTC
int s1, s2 = 0;                 // Recebe o valor obtido por Sensor1 e Sensor2
boolean ResistenciaLigada, BombaLigada = false;       // Status de processo ResistenciaLigada, BombaLigada, 
boolean Congelou, SobreAquec, SobreAquecRes = false;  // Status Placa congelada, Placa e/ou Reservatório com aquecimento.
boolean SobreCargaBomba;        // Status de Sobrecarga na Bomba
boolean ProgAtivo;              // Indica se o horário de programaçao para eventual apoio da resitência está ativo ou não
// Controlador de carga
float t1, t2, t3, c1, p1 = 0;         // Recebe o valor obtido por TensaoPainel, TensaoBateria, CorrenteCarga e TensaoCarga
boolean StatusSobreCarga = false;     // Define se houve sobrecarga no sistema
boolean StatusSubBateria = false;     // Define se a bateria ficou com tensão inferior a SubTensaoBateria
boolean StatusSobreBateria = false;   // Define se a bateria ficou com tensão superior a SobreTensaoBateria
boolean StatusSaida = false;          // Define se a saída foi desligada devido a problemas no sistema
// Reaproveitamento de Água da chuva
boolean StatusCisterna = false;       // Define se tem água na cisterna
boolean StatusValvula = false;        // Define se a válvula está acionada
boolean StatusBombaRecalque = false;  // Define se a Bomba de Recalque está acionada
boolean StatusCxSuperior = false;     // Define se o nível de água na caixa está no máximo
boolean StatusCxInferior = false;     // Define se o nível de água na caixa está no mínimo
boolean SobreCargaBombaRecalque = false;              // Status de SobreCarga na Bomba de Recalque


// Aquecimento Solar - Parametros utilizados para controlar o processo de troca de calor, apoio via resistência e proteções
const int SPResistencia  = 35;  // Set Point para ligar a resistência de apoio                  
const int HSPResistencia = 1;   // Histerese para desligar a resistência de apoio
const int ACPlacas = 3;         // _Temperatura para ligar a proteção de anticongelamento das placas
const int HACPlacas = 1;        // _Histerese para desligar a proteção de anticongelamento das placas
const int SAPlacas = 90;        // _Temperatura para ligar a proteção de sobreaquecimento das placas
const int HSAPlacas = 10;       // _Histerese para desligar a proteção de sobreaquecimento das placas
const int SAReservatorio = 100; // _Temperatura para ligar a proteção de sobreaquecimento do reservatório
const int HSAReservatorio = 10; // _Histerese para desligar a proteçao de sobreaquecimento do reservatório
const int TDiferencialLiga = 8; // _Diferença de temperatura entre as placas e o reservatório para ligar a bomba de calor
const int TDiferencialDesliga = 4;                      // _Diferença de temperatura para desligar a bomba de calor
const int IniHorProg = 12;      // Hora para ativação da resitência de apoio caso necessário (Horário de pico de consumo de água quente)
const int IniMinProg = 23;      // Minuto para ativação da resitência de apoio caso necessário (Horário de pico de consumo de água quente)
const int FimHorProg = 12;      // Hora para desligar a resitência de apoio caso necessário (Horário de pico de consumo de água quente)
const int FimMinProg = 25;      // Minuto para desligar a resitência de apoio caso necessário (Horário de pico de consumo de água quente)

// Controlador de carga - Parametros utilizados para conversão da leitura direta realizada nos ADs e no controle de status do sistema fotovoltaico
const float res_tensao = 0.01855;     // Converte o valor lido no AD dentro da faixa de 0V até 19V. A tensão de testes é de 5V apenas
const float res_corrente = 0.01953;   // Converte o valor lido no AD dentro da faixa de 0A até 20A. 
const int CorrenteSobreCarga = 10;    // Define a corrente máxima de carga para emitir alerta
const float SubTensaoBateria = 11.6;  // Define a tensão minima da bateria antes de cortar a saída
const float SobreTensaoBateria = 14.6;// Define a tensão máxima para cortar a saida e também a carga sobre a própria bateria                    

// Configurações gerais de pinos e shields
void setup()
{
 // Aquecimento Solar
 //Relogio.setDS1302Time(00, 10, 25, 01, 23, 02, 2017);    // Ajuste de data, hora e dia da semana atuais. Pode ser comentado após ajuste inicial do modulo RTC
 pinMode(BombaCalor,OUTPUT);            // Saída Relé para acionamento da bomba de calor e/ou circulação
 pinMode(Resistencia,OUTPUT);           // Saída Relé para acionamento da resistência de apoio do reservatório
 pinMode(TermicoBombaCalor,INPUT);      // Entrada de sinal acusando sobrecarga na bomba de calor 
 digitalWrite(TermicoBombaCalor,HIGH); // Ativação Pull up. Sensor envia sinal baixo.
 // Controlador de carga
 pinMode(ReleFonteAuxiliar,OUTPUT);     // Saída Relé para acionamento da fonte auxiliar AC-DC
 // Reaproveitamento de água da chuva
 pinMode(Boia_cisterna,INPUT);          // Entrada de sinal para monitoramento da cisterna
 pinMode(Caixa_max,INPUT);              // Entrada de sinal para monitoramento de nível máximo na caixa superior
 pinMode(Caixa_min,INPUT);              // Entrada de sinal para monitoramente de nível mínimo na caixa superior
 pinMode(TermicoBombaRecalque,INPUT);   // Entrada de sinal acusando sobrecarga na bomba de recalque
 pinMode(BombaRecalque,OUTPUT);         // Saída Relé para Bomba de recalque
 pinMode(Valvula,OUTPUT);               // Saída Relé para Válvula que irá liberar nível minimo de água potável se não houver agua da chuva disponível
 digitalWrite(Boia_cisterna,HIGH);      // Ativação Pull-up. Boia envia sinal baixo.
 digitalWrite(Caixa_max,HIGH);          // Ativação Pull-up. Boia envia sinal baixo.
 digitalWrite(Caixa_min,HIGH);          // Ativação Pull-up. Boia envia sinal baixo.
 digitalWrite(TermicoBombaRecalque,HIGH); // Ativação Pull up. Sensor envia sinal baixo.
 // Uso comum
 pinMode(LedAquecimento,OUTPUT);        // Led status Aquecimento Solar
 pinMode(LedControlador,OUTPUT);        // Led status Controlador de carga
 pinMode(LedAgua,OUTPUT);               // Led Reaproveitamento água

 //Inicialização gerais
 // Aquecimento Solar 
 digitalWrite(BombaCalor, LOW);
 digitalWrite(Resistencia, LOW);
 // Controlador de carga
 digitalWrite(ReleFonteAuxiliar, LOW);
 // Reaproveitamento de Àgua da chuva
 digitalWrite(BombaRecalque, LOW);
 digitalWrite(Valvula, LOW);
 // Uso comum
 digitalWrite(LedAquecimento, LOW);
 digitalWrite(LedControlador, LOW);
 digitalWrite(LedAgua, LOW);
 
 Serial.begin(9600);
 Serial.println(" ");
 
 //Porta Ethernet
 Ethernet.begin(mac,ip);
 server.begin();
 Serial.begin(9600);
}

// Programa principal 
// Executa as funções aquecimento_solar, controlador_carga e reaproveitamento_agua, mantém o status atualizado e retorna para a função statusled um identificador que pode ser
// 1- Operação Normal, 2- Advertência, 3-Alarme. A função statusled analisa este identificador e permite ao led RGB ficar acesso ou pulsar lento ou rápido a cor que 
// representar uma funcionalidade com advertência ou alarme.
// Na sequencia ocorre a comunicação Ethernet para envio dos dados de status das variáveis globais utiliadas nas funções para a página HTML. O aplicativo Android usará esta página para 
// obter os dados que serão apresentados no mesmo.
void loop()
{
  statusled(aquecimento_solar(),controlador_carga(),reaproveitamento_agua());
  delay(500);
  //Inicializa comunicação Ethernet
  EthernetClient client = server.available();

  //Se a conexão ocorreu com sucesso então envia dados básicos para uma página HTML, acessada via <ipdefinido:8090>
  if (client)
  {
    while(client.connected())
     {
      //Envia resposta com cabeçalho padrão http
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Refresh: 2");//atualiza a página a cada 2 segundos
      client.println();
      //envia o body do arquivo HTML (imagem na tela do PC)
      client.println("<html><body>"); //abre hmtl e body para internet, imagens tela PC
      client.println("<h1> Casa Sustentavel </h1>");
      
      //---------------------------------------------
      //Energia solar
      //---------------------------------------------
      // Envia para a página HTML as informações relacionadas a energia solar. Padroniza as medições com o mesmo número de casas a esquerda para leitura posterior do aplicativo
      client.println("</p>ENERGIA SOLAR </p>");      
      client.print("</p>Tensao do painel fotovoltaico: "); if (t1<10) client.print("0"); client.print(t1); client.println("V");
      client.print("</p>Tensao da Bateria: "); if (t2<10) client.print("0"); client.print(t2); client.println("V");
      client.print("</p>Tensao na Carga: "); if (t3<10) client.print("0"); client.print(t3); client.println("V");
      client.print("</p>Corrente na Carga: "); if (c1<10) client.print("0"); client.print(c1); client.println("A");
      client.print("</p>Potencia na saida: "); if ((t3*c1)<100) client.print("0");client.print(t3*c1); client.println("W");
            
      //verfica status sobre tensão, sem tensão de saída, ok
      client.print("</p>Status da Bateria: ");
      if (StatusSubBateria) client.println("Sub Tensao"); 
      else
       {  if (StatusSobreBateria) client.println("Sobre Tensao");
          else client.println("Bateria OK"); }
     
      //Verifica status da tensão de saída, se é da rede ou não  
      client.print("</p>Tensao de saida: ");
      if (!StatusSaida) client.println("Tensao OK");
      else
       if (StatusSaida) client.println("Energia da Rede Eletrica");
      
      //------------------------------------------------------
      //Aquecimento solar
      //------------------------------------------------------
      // Envia para a página HTML as informações relacionadas a aquecimento solar. Padroniza as medições com o mesmo número de casas a esquerda para leitura posterior do aplicativo
      client.println("</p>AQUECIMENTO SOLAR </p>");      
      
      client.print("</p>Temperatura do Reservatorio: "); 
      if ((s2<100)&&(s2>9)) client.print("0"); if ((s2<9)&&(s2>=0)) client.print("00"); 
      client.print(s2); client.println("oC");
      
      client.print("</p>Temperatura das Placas: ");
      if ((s1<100)&&(s1>9)) client.print("0"); if ((s1<9)&&(s1>=0)) client.print("00");
      client.print(s1); client.println("oC");
      
      //verifica status da bomba de circulação
      client.println("</p> Status Bomba Aquecimento: ");
      if (SobreCargaBomba) client.println("Defeito na Bomba de Aquecimento");
      else
       {  if(BombaLigada) client.println("Bomba de Aquecimento Ligada");
          if(!BombaLigada) client.println("Bomba de Aquecimento Desligada");
       }
      
      //verifica resistencia eletrica ligada ou desligada
      client.print("</p> Status Resistencia Aquecimento: ");
      if (ResistenciaLigada) client.println("Resistencia Ligada");
      else
       if(!ResistenciaLigada) client.println("Resistencia Desligada");
      
      // Verifica sobre aquecimento/congelamento/
      client.println("</p> Status Placas/Reservatorio: ");
      if (Congelou) client.println("Congelamento das placas");
      else
       {  if (SobreAquec) client.println("Sobreaquecimento Placas");
          else 
           if (SobreAquecRes) client.println("Sobreaquecimento Reservatorio"); 
           else client.println("Normal");
       }
           
      
      //------------------------------------------------------
      //Água da chuva
      //------------------------------------------------------
      // Envia para a página HTML as informações relacionadas a reaproveitamento da água da chuva
      client.println("</p> AGUA DA CHUVA </p>");
      
      // verifica status da bomba para escrever no browser
      client.print("<p> Status bomba: ");     
      if (!SobreCargaBombaRecalque) client.println("Defeito na Bomba");
      else
       {  if (StatusBombaRecalque) client.println("Bomba Ligada"); else client.println("Bomba Desligada"); }
      
      // verifica status da água da água da rede para escrever no browser
      client.print("<p> Status Geral: "); if (StatusValvula) client.println("Uso de agua da rede"); else client.println("OK");
      client.print("<p> Status Cisterna: "); if (StatusCisterna) client.println("OK"); else client.println("Vazia");
      client.print("<p> Status Caixa Superior: "); if ((StatusCxSuperior)||(StatusCxInferior)) client.println("OK"); else if (!StatusCxInferior) client.println("Nivel Minimo"); 

      //Fim da comunicação Ethernet
      client.print("</p>");
      client.println("</body></html>");
      delay(1);
      client.stop();
     }
  }
}

// Esta função irá piscar o led RGB de forma rápida ou lenta ou permanecer aceso de acordo com o problema apresentado. Desta forma o usuário poderá consultar o aplicativo 
// para obter maiores detalhes de funcionamento do sistema e ter uma rápida visualização de algum anomalia do sistema.
void statusled(int as, int cc, int ra)
{
 int cont;
 boolean aceso;

 // Caso as 3 funcionalidades estejam ok o led acenderá Branco devido a combinação de core RGB, caso contrário haverá uma identificação de acordo com a funcionalidade.
 if ((as == 1) && (cc == 1) && (ra == 1)) {digitalWrite(LedAquecimento, HIGH); digitalWrite(LedControlador, HIGH); digitalWrite(LedAgua, HIGH); delay(100);}
 else {digitalWrite(LedAquecimento, LOW); digitalWrite(LedControlador, LOW); digitalWrite(LedAgua, LOW);}
 
 // Retorno referente ao aquecimento solar
 // 2 - Resistência ligada - utilizando energia convencional
 // 3 - Bomba de calor com sobrecarga
 switch (as) 
 {
   case 2:      // Led R pista lento. Status Advertência
      for (cont=0; cont <= 1; cont++)
      { if (aceso) { digitalWrite(LedAquecimento, LOW); aceso=false; } else { digitalWrite(LedAquecimento, HIGH); aceso=true; }
        delay(333);       }        
        break;
   case 3:      // Led R pista rápido. Status Alarme
      for (cont=0; cont <= 20; cont++)
      { if (aceso) { digitalWrite(LedAquecimento, LOW); aceso=false; } else { digitalWrite(LedAquecimento, HIGH); aceso=true; }
        delay(16); }        
      break;
   break;
 }

 // Retorno referente ao controlador de carga
 // 2 - Fonte Auxiliar ligada - utilizando energia convencional
 // 3 - Bateria danificada
 switch (cc) 
 {
   case 2:      // Led G pista lento. Status Advertência
      for (cont=0; cont <= 1; cont++)
      { if (aceso) { digitalWrite(LedControlador, LOW); aceso=false; } else { digitalWrite(LedControlador, HIGH); aceso=true; }
        delay(333);       }        
        break;
   case 3:      // Led G pista rápido. Status Alarme
      for (cont=0; cont <= 20; cont++)
      { if (aceso) { digitalWrite(LedControlador, LOW); aceso=false; } else { digitalWrite(LedControlador, HIGH); aceso=true; }
        delay(16); }        
      break;
   break;
 } 

 // Retorno referente ao reaproveitamento da água
 // 2 - Válvula ligada - utilizando água da rede
 // 3 - Bomba de recalque com sobrecarga 
 switch (ra) 
 {
   case 2:      // Led B pista lento. Status Advertência
      for (cont=0; cont <= 1; cont++)
      { if (aceso) { digitalWrite(LedAgua, LOW); aceso=false; } else { digitalWrite(LedAgua, HIGH); aceso=true; }
        delay(333);       }        
        break;
   case 3:      // Led B pista rápido. Status Alarme
      for (cont=0; cont <= 20; cont++)
      { if (aceso) { digitalWrite(LedAgua, LOW); aceso=false; } else { digitalWrite(LedAgua, HIGH); aceso=true; }
        delay(16); }        
      break;
   break;
 }       
}

// AQUECIMENTO SOLAR - Declaração de funcões utilizadas no programa principal
// Linha serial "dd/mm/aaaa  hh:mm  TempReserv: xxoC  TempPlacas: xxoC SP CP BL RL SB SR"
// Data e Hora, Temperatura do Reservatório e das placas 
// Status SP - Sobreaquecimento placas, CP - Congelamento placas, BL - Bomba ligada, RL - Resistência ligada, SM - Sobrecarga Bomba, SR - Sobreaquecimento Reservatorio
int aquecimento_solar()
  {
  int status_aquecimento = 1;   // Recebe a identificação do status geral do aquecimento solar
  
  // Leitura de data e hora atuais
  Relogio.updateTime();
  Serial.print("Data: "); Serial.print(Relogio.dayofmonth); Serial.print("/"); Serial.print(Relogio.month); Serial.print("/"); Serial.print(Relogio.year); Serial.print(" ");
  Serial.print("Hora: "); Serial.print(Relogio.hours); Serial.print(":"); Serial.print(Relogio.minutes); Serial.print(" ");
   
  // Leitura ADs referente temperatura dos sensores
  s1 = (((analogRead(Sensor1)-335)/2.76)-50); 
  s2 = (((analogRead(Sensor2)-335)/4.45)-50);
  Serial.print("Temperatura Placas: "); Serial.print(s1); Serial.print("oC "); Serial.print("Temperatura Reservatorio: "); Serial.print(s2); Serial.print("oC ");
  
  // Leitura do sinal de entrada do relé térmico da Bomba de Calor. Se tiver atuado impede a bomba de ligar e atualiza status SobreCargaBomba
  if (digitalRead(TermicoBombaCalor)==LOW) SobreCargaBomba = true; else SobreCargaBomba = false; 
  
  // Processo principal que irá gerar dados para atualização do status do controlador
  // Teste condicional para Troca de Calor ligando a bomba caso a temperatura das placas esteja superior a do reservatório considerando a constante TDiferencialLiga
  // Mas irá atuar somente se já não houver sobreaquecimento nas placas.
  // Principal funcionalidade do sistema atuando como trocador de calor proporcional a radiação solar incidente nas placas.
  if (((s1-TDiferencialLiga) > s2) && !(BombaLigada) && !(SobreAquec)) BombaLigada = LigaBomba();
  else 
     if (((s1-TDiferencialDesliga) <= s2) && (BombaLigada) && !(SobreAquecRes)) BombaLigada = DesligaBomba();
    
  // Teste condicional para proteção anticongelamento. Aciona a bomba caso a temperatura das placas fique inferior ou igual a constante ACPlacas
  // Este proteção impede que as placas sejam danificadas pelo congelamento dos tubos internos.
  if ((s1 <= ACPlacas) && !(Congelou))
    { LigaBomba(); Congelou = true; }
   else 
     if ((s1 > (ACPlacas+HACPlacas)) && (Congelou))
      { DesligaBomba(); Congelou = false; }
 
  // Teste condicional para proteção contra sobreaquecimento das placas. Desliga a bomba caso a temperatura das placas fique superior ou igual a constante SAPlacas
  // Isso evita que o encanamento de água quente seja danificado pelas altas temperaturas.
  if ((s1 >= SAPlacas) && (BombaLigada))
    { BombaLigada = DesligaBomba(); SobreAquec = true; }
   else
     if ((s1 < (SAPlacas-HSAPlacas)) && !(BombaLigada) && (SobreAquec))
       { BombaLigada = LigaBomba(); SobreAquec = false; }       
     
  // Teste condicional para proteção contra sobreaquecimento do reservatório. Liga a bomba caso a temperatura das placas fique superior ou igual a constante SAReservatorio
  // Mas irá atuar somente se já não houver sobreaquecimento nas placas.
  if ((s2 >= SAReservatorio) && !(BombaLigada) && !(SobreAquec))
    { BombaLigada = LigaBomba(); SobreAquecRes = true; }
   else
     if ((s2 < (SAReservatorio-HSAReservatorio)) && (BombaLigada) && (SobreAquecRes))
       { BombaLigada = DesligaBomba(); SobreAquecRes = false; }       
    
  // Compara horário atual com horário programado inicial para eventual apoio da Resistência e seta a variável progativo em verdadeiro ativando controle de temperatura da resistência
  // Senão compara novamente horário atual com o horário programado final para desativar o eventual apoio por Resistência.
  Relogio.updateTime();
  if ((IniHorProg == (Relogio.hours)) && (IniMinProg == (Relogio.minutes))) ProgAtivo = true;
  else 
    if ((FimHorProg == (Relogio.hours)) && (FimMinProg == (Relogio.minutes))) ProgAtivo = (ResistenciaLigada = DesligaResistencia());  

  // Envia via serial os status SP, CP, BL, RL, SM, SR e altera status geral se a Resistência estiver ligada ou a Bomba sofrer Sobrecarga
  if (SobreAquec) Serial.print("SP "); else Serial.print("-- ");
  if (Congelou) Serial.print("CP "); else Serial.print("-- ");
  if (BombaLigada) Serial.print("BL "); else Serial.print("-- ");
  if (ResistenciaLigada) { Serial.print("RL "); status_aquecimento=2;} else Serial.print("-- ");
  if (SobreCargaBomba) {Serial.print("SB "); status_aquecimento=3;} else Serial.print("-- ");
  if (SobreAquecRes) Serial.println("SR "); else Serial.println("-- ");
  Serial.println(""); 
        
  // Controle de temperatura se no reservatório estiver inferior a SPResistencia considerando ProgAtivo
  if ((ProgAtivo) && (s2 < SPResistencia)) ResistenciaLigada = LigaResistencia();
  else
   if ((ProgAtivo) && (s2 > (SPResistencia - HSPResistencia))) ResistenciaLigada = DesligaResistencia();
  
  return status_aquecimento;
  }

// Liga Bomba e atualiza status da própria bomba como ligado
boolean LigaBomba()
  { digitalWrite(BombaCalor, HIGH);
    return true; }  

// Desliga Bomba e atualiza status da própria bomba como desligado
boolean DesligaBomba()
  { digitalWrite(BombaCalor, LOW);
    return false; }  

// Liga Resistencia e atualiza status da própria resistência como ligado
boolean DesligaResistencia()
  {   digitalWrite(Resistencia, LOW);
      return false; }

// Desiga Resistencia e atualiza status da própria resistência como desligado
boolean LigaResistencia()
  {   digitalWrite(Resistencia, HIGH);
      return true; }

// CONTROLADOR DE CARGA - Declaração de funcões utilizadas no programa principal
// Linha Serial: "Painel: xx.xxV  Bateria: xx.xxV  Carga: xx.xxV  Corrente: xx.xxA  Consumo: xx.xxW  SC UB SB SS -- --"
int controlador_carga()
{
  int status_controlador = 1;   // Recebe a identificação do status geral do controlador de carga
  
  // Leitura ADs referente tensões e corrente de carga do sistema
  t1 = (19-(res_tensao*(analogRead(TensaoPainel)))); //Atribui uma tensão entre 0V e 19V como tensão do painel fotovoltaica. Simulado via LDR 
  t2 = (res_tensao*(analogRead(TensaoBateria)));   // Atribui uma tensão entre 0V e 19V como tensão da bateria
  t3 = (res_tensao*(analogRead(TensaoCarga)));     // Atribui uma tensão entre 0V e 19V como tensão de carga
  c1 = (res_corrente*(analogRead(CorrenteCarga))); // Atribui uma corrente entre 0A e 20A com corrente de carga

  // Apresentação dos dados de tensão, corrente e status obtidos através da leitura dos ADs
  Serial.print("Painel: "); Serial.print(t1); Serial.print("V  "); 
  Serial.print("Bateria: "); Serial.print(t2); Serial.print("V  "); 
  Serial.print("Carga: "); Serial.print(t3); Serial.print("V  "); 
  Serial.print("Corrente: "); Serial.print(c1); Serial.print("A  ");
  Serial.print("Consumo: "); Serial.print((t3*c1)); Serial.print("W  ");

  // As linhas condicionais indicam se houve sobrecarga na saída, tensões inferiores ou superiores na bateria ou se saída ficou sem tensão
  // SC - Sobrecarga na saída, UB - Subtensão na saída, SB - Sobretensão na saída, SS - Saída desligada
  if (c1 > CorrenteSobreCarga) { StatusSobreCarga=true; Serial.print("SC "); } else { StatusSobreCarga=false; Serial.print("-- "); }
  if (t2 < SubTensaoBateria) { StatusSubBateria=true; Serial.print("UB "); status_controlador=3;} else { StatusSubBateria=false; Serial.print("-- "); } 
  if (t2 > SobreTensaoBateria) { StatusSobreBateria=true; Serial.print("SB "); status_controlador=3;} else { StatusSobreBateria=false; Serial.print("-- "); } 
  if (t3 < 10) { StatusSaida=true; Serial.println("SS "); digitalWrite(ReleFonteAuxiliar,HIGH); status_controlador=2; } else { StatusSaida=false; Serial.println("-- "); digitalWrite(ReleFonteAuxiliar,LOW); } 
  Serial.println("");
  return status_controlador;
}

// REAPROVEITAMENTO DE AGUA DA CHUVA - Declaração de funcões utilizadas no programa principal
// Linha Serial: "Cisterna: OK ou --  Caixa Superior: OK ou --  Caixa Inferior: OK ou --  Status: BL VL SB"
int reaproveitamento_agua()
  {
  int status_reaproveitamento = 1;   // Recebe a identificação do status geral do reaproveitamento de água
  
  // Status das bóias para atualização no sistema
  if (digitalRead(Boia_cisterna)==HIGH) StatusCisterna = true; else StatusCisterna = false;
  if (digitalRead(Caixa_max)==HIGH) StatusCxSuperior = true; else StatusCxSuperior = false;
  if (digitalRead(Caixa_min)==HIGH) StatusCxInferior = true; else StatusCxInferior = false;
  if (digitalRead(TermicoBombaRecalque)==HIGH) SobreCargaBombaRecalque = true; else SobreCargaBombaRecalque = false;
  
  // Quando tem água na cisterna
  if (StatusCisterna) 
    {    
    StatusValvula = DesligaValvula(); 
    if (StatusCxSuperior) StatusBombaRecalque = DesligaBombaRecalque(); else StatusBombaRecalque = LigaBombaRecalque();
    }
  //Quando não tem água na cisterna
    else
    {       
    StatusBombaRecalque = DesligaBombaRecalque();
    if (StatusCxInferior) StatusValvula = DesligaValvula(); else if (!StatusCxSuperior) StatusValvula = LigaValvula(); 
    }

  Serial.print("Cisterna: "); if (StatusCisterna) Serial.print("OK"); else Serial.print("--"); 
  Serial.print("  Caixa Superior: "); if (StatusCxSuperior) Serial.print("OK"); else Serial.print("--");
  Serial.print("  Caixa Inferior: "); if (StatusCxInferior) Serial.print("OK"); else Serial.print("--"); 
  Serial.print("  Status: ");
  if (StatusBombaRecalque) Serial.print("BL "); else Serial.print("-- ");
  if (StatusValvula) { Serial.print("VL "); status_reaproveitamento=2;} else Serial.print("-- ");
  if (SobreCargaBombaRecalque) Serial.println("-- "); else { Serial.println("SB "); status_reaproveitamento=3; }
  Serial.println("");
  return status_reaproveitamento; 
  }

// Desliga a Vávula e atualiza status da própria válvula como desligado
boolean DesligaValvula()
  { digitalWrite(Valvula,LOW); return false; }  

// Liga a Vávula e atualiza status da própria válvula como ligado
boolean LigaValvula()
  { digitalWrite(Valvula,HIGH); return true; }   

// Desliga a Bomba de Recalque e atualiza status da própria bomba como desligado
boolean DesligaBombaRecalque()
  {  digitalWrite(BombaRecalque, LOW); return false; }  

// Liga a Bomba de Recalque e atualiza status da própria bomba como ligado
boolean LigaBombaRecalque()
  {  digitalWrite(BombaRecalque, HIGH); return true; }  



  











