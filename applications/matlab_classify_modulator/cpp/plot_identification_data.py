import numpy as np
import random
from dash import Dash, html, dcc, dash_table
import dash
import dash_bootstrap_components as dbc
import plotly.graph_objs as go
import plotly.express as px
import pandas as pd
import scipy.io
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score
from sklearn.metrics import mean_squared_error, r2_score
from sklearn.metrics import roc_curve, roc_auc_score
import os
import time
import adi
import scipy.io

# Constants
LABELS = ['16QAM', '64QAM', '8PSK', 'BPSK', 'CPFSK', 'GFSK', 'PAM4', 'QPSK']
MODULATION_MAP = {
    "1": "16QAM",
    "2": "64QAM",
    "3": "8PSK",
    "5": "BPSK",
    "6": "CPFSK",
    "8": "GFSK",
    "9": "PAM4",
    "10": "QPSK"
}
CNN_MATRIX_ASSOCIATION = {
    1: 0, 2: 1, 3: 2, 5: 3, 6: 4, 8: 5, 9: 6, 10: 7
}
FILE_PATH = '../../../build/matlab_classify_modulator/modulation_results.txt'


class ModulationClassificationApp:
    def __init__(self):
        self.confusion_matrix = np.zeros((len(LABELS), len(LABELS)))
        self.iteration = 0
        self.last_sequence = -1
        self.truth_vector = []
        self.estimated_vector = []
        self.probability_vector = []
        self.sdr = None
        self.current_truth = 0
        # Initialize with empty data
        self.df = pd.DataFrame(dict(x=[0], y=[0]))

    def setup_sdr(self):
        """Initialize PlutoSDR if available"""
        try:
            self.sdr = adi.ad9361(uri="ip:192.168.2.1")
            self.sdr.tx_lo = 2400000000
            self.sdr.tx_cyclic_buffer = True
            self.sdr.tx_hardwaregain_chan0 = -10
            self.sdr.tx_rf_bandwidth = 20000000
            self.sdr.tx_sample_rate = 1000000
        except Exception as e:
            print(f"Could not initialize SDR: {e}")
            self.sdr = None

    def get_new_data(self, file_name):
        """Check for new data in the file based on sequence number"""
        try:
            if os.path.exists(file_name):
                with open(file_name, 'r') as file:
                    lines = file.readlines()
                    if len(lines) >= 4:
                        sequence_line = lines[1].strip()
                        current_sequence = int(sequence_line.split(': ')[1])

                        if current_sequence > self.last_sequence:
                            self.last_sequence = current_sequence
                            return True, lines
        except (IOError, OSError, IndexError, ValueError) as e:
            print(f"File read error (will retry): {e}")
        return False, None

    def load_modulation_data(self):
        """Load all modulation data files"""
        return {
            '8PSK': scipy.io.loadmat('modulated_data/mod_8PSK.mat'),
            '16QAM': scipy.io.loadmat('modulated_data/mod_16QAM.mat'),
            '64QAM': scipy.io.loadmat('modulated_data/mod_64QAM.mat'),
            'BPSK': scipy.io.loadmat('modulated_data/mod_BPSK.mat'),
            'CPFSK': scipy.io.loadmat('modulated_data/mod_CPFSK.mat'),
            'GFSK': scipy.io.loadmat('modulated_data/mod_GFSK.mat'),
            'PAM4': scipy.io.loadmat('modulated_data/mod_PAM4.mat'),
            'QPSK': scipy.io.loadmat('modulated_data/mod_QPSK.mat')
        }

    def get_cnn_data(self):
        """Generate new modulation data and check for classification results"""
        # Load modulation data
        mod_data = self.load_modulation_data()

        # Generate random truth value
        self.current_truth = random.choice([1, 2, 3, 5, 6, 8, 9, 10])
        modulation_type = MODULATION_MAP[str(self.current_truth)]
        print(modulation_type)

        # Get corresponding data
        data = mod_data[modulation_type]['rx']
        data = data.flatten()
        iq_real = np.int16(np.real(data) * 2**12-1)
        iq_imag = np.int16(np.imag(data) * 2**12-1)
        iq = iq_real + 1j * iq_imag

        # Transmit via PlutoSDR if available
        if self.sdr is not None:
            self.sdr.tx_enabled_channels = [0]
            self.sdr.tx_destroy_buffer()
            self.sdr.tx(iq)

        # Give C++ time to process
        time.sleep(0.1)

        print(
            f"Checking for new file updates... (last_sequence: {self.last_sequence})")
        has_new_data, lines = self.get_new_data(FILE_PATH)

        if has_new_data:
            print("New data detected!")
            try:
                self.truth_vector.append(self.current_truth)
                print(f"File contents: {lines}")

                # Parse data
                modulation_line = lines[2].strip()
                estimated = int(modulation_line.split(': ')[1])
                print(
                    f"Read modulation: {estimated} from line: '{modulation_line}'")

                confidence_line = lines[3].strip()
                probability = float(confidence_line.split(': ')[1])

                self.estimated_vector.append(estimated)
                self.probability_vector.append(probability)

                # Update confusion matrix with current data
                self.update_confusion_matrix(self.current_truth, estimated)

            except (IndexError, ValueError) as e:
                print(f"Error parsing file data: {e}")
                if self.truth_vector:
                    self.truth_vector.pop()
        else:
            print("No new data found")

        # Clean up old data
        if len(self.estimated_vector) > 10:
            self.truth_vector = []
            self.estimated_vector = []
            self.probability_vector = []

    def update_confusion_matrix(self, truth, estimated):
        """Update confusion matrix with new truth/estimated pair"""
        truth_idx = CNN_MATRIX_ASSOCIATION.get(truth)
        estimated_idx = CNN_MATRIX_ASSOCIATION.get(estimated)

        if truth_idx is not None and estimated_idx is not None:
            self.confusion_matrix[truth_idx][estimated_idx] += 1
            print(
                f"Updated confusion matrix: truth={truth}(idx:{truth_idx}), estimated={estimated}(idx:{estimated_idx})")
        else:
            print(f"Invalid mapping: truth={truth}, estimated={estimated}")

        self.iteration += 1
        if self.iteration == 50:
            self.confusion_matrix = np.zeros((len(LABELS), len(LABELS)))
            self.iteration = 0

    def update_modulated_data(self):
        """Update the modulated data plot"""
        if self.current_truth != 0:
            mod_file = scipy.io.loadmat(
                f'modulated_data/mod_{MODULATION_MAP[str(self.current_truth)]}.mat')
            modulated_data = mod_file['rx']
            modulated_data_re = np.real(modulated_data.flatten())
            time_axis = np.arange(0, len(modulated_data))

            self.df = pd.DataFrame(dict(x=time_axis, y=modulated_data_re))

    def plot_modulated_data(self):
        """Create plot of modulated data"""
        truth_label = MODULATION_MAP.get(str(
            self.current_truth), "Unknown") if self.current_truth != 0 else "Initializing..."
        fig = px.line(self.df, x="x", y="y",
                      title=f"Modulated signal - Truth: {truth_label}", line_shape='linear')
        fig.update_traces(line=dict(color='#009FBD'))
        fig.update_layout(plot_bgcolor='white',
                          xaxis=dict(showgrid=True, gridcolor='gray'),
                          yaxis=dict(showgrid=True, gridcolor='gray'))
        return fig

    def plot_confusion_matrix(self):
        """Create confusion matrix plot"""
        fig = go.Figure(data=go.Heatmap(
            z=self.confusion_matrix,
            x=LABELS,
            y=LABELS,
            colorscale='Blues'
        ))
        fig.update_layout(
            title='Confusion Matrix for the Modulation Identification',
            xaxis=dict(title='Estimated Modulation'),
            yaxis=dict(title='True Modulation')
        )
        return fig

    def update_table(self):
        """Update metrics table"""
        if len(self.truth_vector) > 2:
            accuracy = accuracy_score(self.truth_vector, self.estimated_vector)
            precision = precision_score(self.truth_vector, self.estimated_vector,
                                        average='weighted', zero_division=1)
            recall = recall_score(self.truth_vector, self.estimated_vector,
                                  average='weighted', zero_division=1)
            f1 = f1_score(self.truth_vector,
                          self.estimated_vector, average='weighted')
            mse = mean_squared_error(self.truth_vector, self.estimated_vector)
            r2 = r2_score(self.truth_vector, self.estimated_vector)
        else:
            accuracy = precision = recall = f1 = mse = r2 = 0

        return [{
            'column-1': f'{accuracy:.2f}',
            'column-2': f'{precision:.2f}',
            'column-3': f'{recall:.2f}',
            'column-4': f'{f1:.2f}',
            'column-5': f'{mse:.2f}',
            'column-6': f'{r2:.2f}'
        }]


# Create global app instance
classification_app = ModulationClassificationApp()
app = Dash(__name__)

app.layout = html.Div([
    html.H1("High-Performance Analog Meets AI",
            style={'textAlign': 'center', 'backgroundColor': '#00427a',
                   'color': 'white', 'fontSize': '36px'}),

    html.Div([
        html.Div(dcc.Graph(id='confusion-matrix'),
                 style={'width': '40%', 'display': 'inline-block', 'margin-left': '5%'}),
        html.Div(dcc.Graph(id='modulated-data'),
                 style={'width': '40%', 'display': 'inline-block', 'margin-left': '10%'})
    ], style={'width': '90%', 'margin-left': '5%', 'margin-top': '1%',
              'backgroundColor': 'transparent', 'border-radius': '10px',
              'box-shadow': '2px 2px 5px rgba(0, 0, 0, 0.1)'}),

    html.Div(
        dash_table.DataTable(
            id='example-table',
            columns=[
                {'name': 'Accuracy', 'id': 'column-1'},
                {'name': 'Precision', 'id': 'column-2'},
                {'name': 'Recall', 'id': 'column-3'},
                {'name': 'F1 rate', 'id': 'column-4'},
                {'name': 'MSE rate', 'id': 'column-5'},
                {'name': 'R2 rate', 'id': 'column-6'}
            ],
            data=[{}],
            style_table={'width': '90%', 'margin-left': '5%', 'margin-top': '5%',
                         'border-radius': '50px', 'box-shadow': '4px 4px 10px rgba(0, 0, 0, 0.1)'},
            style_cell={'textAlign': 'center', 'fontSize': '16px'},
            style_header={'backgroundColor': '#1E4056', 'color': 'white'}
        )
    ),

    dcc.Interval(
        id='interval-component',
        interval=5*1000,  # in milliseconds
        n_intervals=0
    )
])


@app.callback(
    [dash.dependencies.Output('confusion-matrix', 'figure'),
     dash.dependencies.Output('modulated-data', 'figure'),
     dash.dependencies.Output('example-table', 'data')],
    [dash.dependencies.Input('interval-component', 'n_intervals')]
)
def update_graph_live(n):
    classification_app.get_cnn_data()
    classification_app.update_modulated_data()
    table_data = classification_app.update_table()
    return (classification_app.plot_confusion_matrix(),
            classification_app.plot_modulated_data(),
            table_data)


if __name__ == '__main__':
    # Initialize SDR
    classification_app.setup_sdr()
    app.run(debug=True)
